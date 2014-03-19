#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import time

# Internal:
from moosecat.gtk.utils import plyr_cache_to_pixbuf
from moosecat.gtk.widgets import StarSlider
from moosecat.boot import g

# External:
from gi.repository import Gtk, Gdk, GdkPixbuf, Pango, GLib


###########################################################################
#                            Utility Function                             #
###########################################################################

def sizeof_fmt(num):
    """Format a number of bytes to a human-readable string with proper metric.
    """
    format_it = '{:3.1f}{}'.format
    for x in ('bytes', 'KB', 'MB', 'GB'):
        if -1024 < num < 1024:
            return format_it(num, x)
        num /= 1024
    return format_it(num, 'TB')


def format_time(timestamp):
    """Format the time in `timestamp` (as Unix-Timestamp) as a nice string.
    """
    return time.strftime(
        '%d.%m.%Y - %H:%m', time.localtime(timestamp)
    )


def listbox_create_labelrow(name, widget, include_colon=True, bold=True):
    """Add a l|r-hbox-combination to a ListBox.

    :param name: The name of left label (written in bold)
    :param widget: The widget to display on the right side.
    :param include_colon: Wether to include a colon at the end of the left side.
    :param bold: Wether to render the label font bold.
    """
    bold_start, bold_end = ('<b>', '</b>') if bold else ('', '')
    if isinstance(name, Gtk.Label):
        name_label = name
    else:
        name_label = Gtk.Label(bold_start + name + '{colon}'.format(
            colon=':' if include_colon else ''
        ) + bold_end)
        name_label.set_use_markup(True)

    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=50)
    box.pack_start(name_label, False, True, 0)
    box.pack_end(widget, False, True, 0)

    row = Gtk.ListBoxRow()
    row.add(box)
    return row


def create_rating_slider(cache):
    """Create a readily configured rating slider, with an appropiate size.
    """
    star_slider = StarSlider(stars=5)

    # This is a bit of a hack:
    def _set_rating():
        star_slider.stars = cache.rating

    GLib.idle_add(_set_rating)

    star_slider.set_size_request(17 * star_slider.width_multiplier(), 10)
    return star_slider


def add_label_to_grid(grid, index, topic, widget):
    """Add a `widget` and a label with `topic` to the grid at row-index `index`
    """
    left = Gtk.Label('<b>{}:</b>'.format(topic))

    left.set_use_markup(True)
    left.set_halign(Gtk.Align.START)
    left.set_hexpand(True)

    # Make it jump to the right:
    widget.set_halign(Gtk.Align.END)

    # Now put it on the same row, both with a singular spacing.
    grid.attach(left, 0, index, 1, 1)
    grid.attach(widget, 1, index, 1, 1)

    return left


def result_change_rating(slider, query, cache, control_box):
    """No utility function, but commonly used as callback for the
    rating-slider.

    Sets the selected rating (on the slider) on the cache and saves it in the
    cache.
    """
    cache.rating = int(round(slider.stars))
    g.meta_retriever.database.edit(query, cache)
    #g.meta_retriever.database.replace(cache.checksum, query, cache)
    control_box.settable = False


def construct_heading(query):
    me = query.get_type
    if me in ['guitartabs', 'lyrics']:
        return '{m} of {t} by {a}'.format(
            m=me.capitalize(),
            t=query.title.capitalize(), a=query.artist.capitalize()
        )
    elif me in ['relations', 'tags']:
        return '{m} for {v}'.format(
            m=me.capitalize(),
            v=(query.artist or query.album or query.title).capitalize()
        )
    elif me in ['artistbio', 'albumreview']:
        return '{m} for {a}'.format(
            m=me.capitalize(),
            a=(query.album or query.artist).capitalize()
        )


###########################################################################
#                                 Widgets                                 #
###########################################################################


class DottedSeparator(Gtk.DrawingArea):
    """Simple widget that provides a dotted, horizontal separator"""
    def __init__(self, height=1.5):
        """
        :param height: The height of the bar in pixels.
        """
        Gtk.DrawingArea.__init__(self)

        # DrawingAreas have no size, so allocate some:
        self.set_size_request(-1, height)
        self.connect('draw', self._on_draw)

    def _on_draw(self, area, ctx):
        alloc = area.get_allocation()
        h2 = alloc.height / 2

        # Slightly transparent black:
        ctx.set_source_rgba(0, 0, 0, 0.8)

        # 4px on, 4px off
        ctx.set_dash([4, 4], 0)

        ctx.set_line_width(h2)
        ctx.move_to(0, h2)
        ctx.line_to(alloc.width, h2)
        ctx.stroke()


class ControlButtonsWidget(Gtk.Box):
    """Widget that implements the three buttons in the Metadata-Items.

    The possible actions are: Save, Delete, Set.
    The latter two are exclusive. Only one is sensitive at a time.
    """
    def __init__(self, query, cache):
        # Make the box linked, so all three buttons blend.
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)
        self.get_style_context().add_class(Gtk.STYLE_CLASS_LINKED)

        # Set all four mmargins at once.
        self.set_property('margin', 4)

        # Set Button:
        self._bts = Gtk.Button.new_from_icon_name(
            'gtk-apply', Gtk.IconSize.BUTTON
        )
        self._bts.set_has_tooltip(True)
        self._bts.set_tooltip_text('Set the Coverart to the database')
        self._bts.set_sensitive(not cache.is_cached)

        # Remove button:
        self._btd = Gtk.Button.new_from_icon_name(
            'gtk-remove', Gtk.IconSize.BUTTON
        )
        self._btd.set_has_tooltip(True)
        self._btd.set_tooltip_text('Delete the Coverart from the database')
        self._btd.set_sensitive(cache.is_cached)

        # Save Button:
        self._sav = Gtk.Button.new_from_icon_name(
            'gtk-floppy', Gtk.IconSize.BUTTON
        )
        self._sav.set_has_tooltip(True)
        self._sav.set_tooltip_text('Save the item to disk.')

        # Signals:
        self._bts.connect('clicked', self._result_set, query, cache, self._btd)
        self._btd.connect('clicked', self._result_del, query, cache, self._bts)
        self._sav.connect('clicked', self._on_show_save_dialog, query, cache)

        # Packing:
        self.pack_end(self._bts, False, False, 0)
        self.pack_end(self._btd, False, False, 0)
        self.pack_end(self._sav, False, False, 0)
        self.set_halign(Gtk.Align.END)
        self.set_valign(Gtk.Align.END)

    def _result_set(self, button, query, cache, del_button):
        g.meta_retriever.database.replace(cache.checksum, query, cache)
        del_button.set_sensitive(True)
        button.set_sensitive(False)

    def _result_del(self, button, query, cache, set_button):
        g.meta_retriever.database.replace(cache.checksum, None, None)
        set_button.set_sensitive(True)
        button.set_sensitive(False)

    def _on_show_save_dialog(self, button, query, cache):
        window = None
        if hasattr(g, 'gtk_main_window'):
            window = g.gtk_main_window

        # Create a new dialog for saving:
        dialog = Gtk.FileChooserDialog(
            'Save Metadata',
            window,
            Gtk.FileChooserAction.SAVE,
            (
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE, Gtk.ResponseType.OK
            )
        )
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            cache.write(dialog.get_filename())
        dialog.destroy()

    @property
    def settable(self):
        return self._bts.is_sensitive()

    @settable.setter
    def settable(self, state):
        self._bts.set_sensitive(state)
        self._btd.set_sensitive(not state)


def show_in_image_viewer(widget, event, pixbuf):
    kill_it = lambda win, *_: win.close()

    # A normal Toplevel Window
    popup = Gtk.Window()

    # Make focus-out-event possible:
    popup.set_events(popup.get_events() | Gdk.EventMask.FOCUS_CHANGE_MASK)

    # Make it dissapear when out of focus or sending a close
    popup.connect('focus-out-event', kill_it)
    popup.connect("destroy", kill_it)

    # Make it show the Cover
    popup.add(CoverWidget(
        pixbuf, max_width=-1, max_height=-1, scale=False
    ))

    # Make interaction with the rest not possible
    # (could lead to multiple spawned popups)
    popup.set_modal(True)

    # Make it act like a dialog for the WM
    popup.set_type_hint(Gdk.WindowTypeHint.DIALOG)

    # Most WMs do not show dialogs in taskbars, but to be sure:
    popup.set_skip_taskbar_hint(True)
    popup.set_skip_pager_hint(True)

    # Position the thing at your mouse
    popup.set_position(Gtk.WindowPosition.MOUSE)

    # Only available when starting the main, not the test-main.
    if hasattr(g, 'gtk_main_window'):
        popup.set_transient_for(g.gtk_main_window)

    popup.show_all()


# TODO: Make this a widgets.class
class CoverWidget(Gtk.EventBox):
    def __init__(self, pixbuf, max_width=200, max_height=-1, scale=True):
        Gtk.EventBox.__init__(self)

        area = Gtk.DrawingArea()
        area.connect('draw', self._on_draw_background, pixbuf)

        self.connect('button-press-event', show_in_image_viewer, pixbuf)

        self._scale = scale
        if scale is True:
            self.set_size_request(max_width, max_height)
        else:
            self.set_size_request(
                pixbuf.get_width(),
                pixbuf.get_height()
            )

        self.add(area)

    def _on_draw_background(self, widget, ctx, pixbuf):
        alloc = widget.get_allocation()
        width, height = pixbuf.get_width(), pixbuf.get_height()

        # Adjust the height allocated by the cover:
        if self._scale:
            fac = float(alloc.width) / max(width, height)
            asp_width, asp_height = fac * width, fac * height
            self.set_size_request(alloc.width, asp_height)
        else:
            asp_width, asp_height = width, height

        # Prevent overlarge images:
        screen = Gdk.Screen.get_default()
        screen_w, screen_h = screen.get_width(), screen.get_height()

        if asp_width > screen_w:
            asp_height *= screen_w / asp_width
            asp_width = screen_w

        if asp_height > screen_h:
            asp_width *= screen_h / asp_height
            asp_height = screen_h

        if self._scale is False:
            self.set_size_request(asp_width, asp_height)

        # Black outline border.
        border = 5

        # Black it out:
        center_rect = int((alloc.height - asp_height) / 2)
        ctx.set_source_rgb(0, 0, 0)
        ctx.rectangle(0, center_rect, asp_width, asp_height)
        ctx.fill()

        # Now calculate the image space and scale it:
        asp_width -= 2 * border
        asp_height -= 2 * border
        thumbnail = pixbuf.scale_simple(
            asp_width, asp_height,
            GdkPixbuf.InterpType.HYPER
        )

        # Render it in the center:
        Gdk.cairo_set_source_pixbuf(
            ctx, thumbnail, border, center_rect + border
        )
        ctx.rectangle(
            border,
            center_rect + border,
            asp_width,
            asp_height
        )
        ctx.fill()


class CoverTemplate(Gtk.Box):
    def __init__(self, query, cache, max_width=200):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        # Convert the cache to a pixbuf and configure the coverart widget:
        pixbuf = plyr_cache_to_pixbuf(cache)
        cover = CoverWidget(pixbuf)
        cover.set_property('margin', 3)

        # Configure the StarSlider for the Rating:
        star_slider = create_rating_slider(cache)

        # Now add the Metadata:
        lsbox = Gtk.ListBox()
        lsbox.set_selection_mode(Gtk.SelectionMode.NONE)
        lsbox.add(
            listbox_create_labelrow(
                'Provider', Gtk.LinkButton.new_with_label(
                    cache.source_url, cache.provider
                )
            )
        )
        lsbox.add(
            listbox_create_labelrow('Size', Gtk.Label(
                '{w}x{h}px ({sz} Bytes)'.format(
                    w=pixbuf.get_width(),
                    h=pixbuf.get_height(),
                    sz=sizeof_fmt(cache.size)
                )
            ))
        )
        lsbox.add(
            listbox_create_labelrow('Format', Gtk.Label(
                cache.image_format.upper()
            ))
        )
        lsbox.add(
            listbox_create_labelrow('Timestamp', Gtk.Label(
                format_time(cache.timestamp)
            ))
        )
        lsbox.add(
            listbox_create_labelrow('Rating', star_slider)
        )

        control_box = ControlButtonsWidget(query, cache)

        star_slider.connect(
            'percent-change', result_change_rating, query, cache, control_box
        )

        # The Right Part of the widget
        overlay = Gtk.Overlay()
        overlay.add(lsbox)
        overlay.add_overlay(control_box)

        self.pack_start(cover, False, False, 0)
        self.pack_start(overlay, True, True, 0)

        self.set_size_request(-1, 180)


class TextTemplate(Gtk.Overlay):
    def __init__(self, query, cache, bottom_bar_height=65):
        Gtk.Overlay.__init__(self)

        control_box = ControlButtonsWidget(query, cache)

        bottom_box = Gtk.HBox()

        # Configure the position of the Box in the Overlay:
        bottom_box.set_halign(Gtk.Align.END)

        # This empty label servers as a Paintable white background.
        # We just position it and paint it white.
        color_label = Gtk.Label()
        color_label.override_background_color(
            0, Gdk.RGBA(1, 1, 1, 1)
        )

        color_label.set_size_request(-1, bottom_bar_height)
        color_label.set_halign(Gtk.Align.FILL)
        color_label.set_valign(Gtk.Align.END)

        bottom_box.pack_end(control_box, True, True, 0)

        text_buffer = Gtk.TextBuffer()

        fg_color = Gdk.RGBA(0.75, 0.75, 0.75, 1.0)
        bg_color = Gdk.RGBA(0.15, 0.15, 0.15, 1.0)

        # Create some tags for formatting the text:
        underlined = text_buffer.create_tag(
            'underlined',
            scale=1.5,
            background_full_height=True,
            paragraph_background_rgba=bg_color,
            foreground_rgba=fg_color,
            background_rgba=bg_color,
            underline=Pango.Underline.SINGLE
        )
        italic = text_buffer.create_tag(
            'italic',
            style=Pango.Style.ITALIC,
            background_full_height=True,
            paragraph_background_rgba=bg_color,
            foreground_rgba=fg_color,
            background_rgba=bg_color,
            family='serif',
            scale=1.25
        )

        heading = construct_heading(query)

        # Now insert the text formatted in a nice way.
        if heading is not None:
            text_buffer.insert_with_tags(
                text_buffer.get_start_iter(),
                heading + '\n\n',
                underlined
            )
        # Insert the main lyrics:
        text_buffer.insert_with_tags(
            text_buffer.get_end_iter(),
            cache.data.decode('utf-8'),
            italic
        )
        # Insert a text padding (sorry, a hack more or less)
        text_buffer.insert_with_tags(
            text_buffer.get_end_iter(),
            '\n' * 5,
            italic
        )

        text_buffer.connect(
            'changed',
            self._on_textbuffer_changed,
            cache,
            control_box
        )

        scw = Gtk.ScrolledWindow()
        scw.add(Gtk.TextView.new_with_buffer(text_buffer))

        self.add(scw)
        self.add_overlay(color_label)
        self.add_overlay(bottom_box)

        grid = Gtk.Grid()
        grid.set_margin_bottom(3)

        link_button = Gtk.LinkButton.new_with_label(
            cache.source_url, cache.provider
        )

        add_label_to_grid(grid, 0, 'Provider', link_button)
        add_label_to_grid(grid, 1, 'Timestamp',  Gtk.Label(
            format_time(cache.timestamp)
        ))

        star_slider = create_rating_slider(cache)
        star_slider.connect(
            'percent-change', result_change_rating, query, cache, control_box
        )

        add_label_to_grid(grid, 2, 'Rating', star_slider)

        # Position the whole grid on the bottom, but leave space for
        # the control_box widget
        grid.set_halign(Gtk.Align.FILL)
        grid.set_valign(Gtk.Align.END)
        grid.set_margin_right(130)

        # Now add a nice visual line to the text-content:
        sep = Gtk.HSeparator()
        sep.set_margin_bottom(bottom_bar_height)
        sep.set_halign(Gtk.Align.FILL)
        sep.set_valign(Gtk.Align.END)

        self.add_overlay(sep)
        self.add_overlay(grid)

        newlines = cache.data.count(b'\n') + 5
        self.set_size_request(-1, 100 + newlines * 20)

    def _on_textbuffer_changed(self, textbuffer, cache, control_box):
        control_box.settable = True
        text = textbuffer.get_text(
            textbuffer.get_start_iter(),
            textbuffer.get_end_iter(),
            True
        )
        cache.data = text.encode('utf-8')


class ListTemplate(Gtk.Box):
    def __init__(self, query, cache):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        control_box = ControlButtonsWidget(query, cache)
        control_box.set_property('margin', 0)

        lsbox = Gtk.ListBox()
        lsbox.set_selection_mode(Gtk.SelectionMode.NONE)

        duration_label = '{}\n{:02}:{:02}'.format(
            cache.data.decode('utf-8'),
            int(cache.duration / 60),
            cache.duration % 60
        )
        lsbox.add(listbox_create_labelrow(
            duration_label, Gtk.Label(''),
            include_colon=False
        ))

        overlay = Gtk.Overlay()
        overlay.add(lsbox)
        overlay.add_overlay(control_box)

        self.pack_start(overlay, True, True, 0)
        self.set_size_request(-1, 40)


if __name__ == '__main__':
    # Stdlib:
    import sys

    # Internal:
    from moosecat.gtk.runner import main
    from moosecat.metadata import configure_query

    with main(metadata=True) as win:
        qry1 = configure_query('cover', sys.argv[1], sys.argv[2])
        qry1.database = g.meta_retriever.database
        cache1 = qry1.commit()[0]

        qry2 = configure_query('lyrics', sys.argv[3], title=sys.argv[4])
        qry2.database = g.meta_retriever.database
        cache2 = qry2.commit()[0]

        qry3 = configure_query('tracklist', 'Metallica', 'Load', amount=20)
        qry3.database = g.meta_retriever.database
        cache3 = qry3.commit()

        box = Gtk.VBox()
        box.pack_start(TextTemplate(qry2, cache2), False, False, 0)
        box.pack_start(DottedSeparator(), False, False, 0)
        box.pack_start(CoverTemplate(qry1, cache1), False, False, 0)

        for x in cache3:
            box.pack_start(DottedSeparator(), False, False, 0)
            box.pack_start(ListTemplate(qry3, x), False, False, 0)

        scw = Gtk.ScrolledWindow()
        scw.add(box)
        win.add(scw)
        win.set_default_size(500, 450)
