#!/usr/bin/env python
# encoding: utf-8

from moosecat.boot import g
import moosecat.metadata as metadata

from gi.repository import Gtk, GLib, GObject
import plyr


import moosecat.gtk.metadata.templates as templates
from moosecat.gtk.utils.drawing import draw_center_text


def select_template(query, cache):
    if cache.is_image:
        return templates.CoverTemplate(query, cache)
    elif query.get_type in ['tracklist', 'albumlist']:
        return templates.ListTemplate(query, cache)
    else:
        return templates.TextTemplate(query, cache)


def create_get_type_combobox():
    list_store = Gtk.ListStore(str, str)
    for key in sorted(plyr.PROVIDERS.keys()):
        list_store.append(('gtk-find', key.capitalize()))

    combo = Gtk.ComboBox.new_with_model(list_store)

    renderer = Gtk.CellRendererPixbuf()
    cell = Gtk.CellRendererPixbuf()
    combo.pack_start(cell, False)
    combo.add_attribute(cell, 'icon-name', 0)

    renderer = Gtk.CellRendererText()
    combo.pack_start(renderer, True)
    combo.add_attribute(renderer, 'text', 1)

    combo.set_active(0)
    return combo


def make_entry_row(name, icon_name, widget=None):
    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)

    image = Gtk.Image.new_from_icon_name(icon_name, Gtk.IconSize.BUTTON)
    label = Gtk.Label('<b>{}:</b>'.format(name))
    field = widget or Gtk.Entry()
    if widget is None:
        field.set_has_frame(False)
    field.set_hexpand(True)

    image.set_margin_right(4)

    label.set_use_markup(True)
    label.set_alignment(0, 0.5)
    label.set_size_request(42, -1)

    box.pack_start(image, False, False, 1)
    box.pack_start(label, False, False, 1)
    box.pack_start(field, True, True, 1)

    return box, field


class SearchButton(Gtk.Button):
    def __init__(self, cancel_button):
        Gtk.Button.__init__(self)
        self.get_style_context().add_class(Gtk.STYLE_CLASS_SUGGESTED_ACTION)

        self._cancel_button = cancel_button

        image = Gtk.Image.new_from_icon_name(
            'gtk-find-and-replace', Gtk.IconSize.MENU
        )
        image.set_alignment(0, 0.5)

        self._label = Gtk.Label('Search')
        self._label.set_alignment(0, 0.5)

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(image, False, False, 2)
        box.pack_start(Gtk.VSeparator(), False, False, 2)
        box.pack_start(self._label, True, True, 2)

        self._level_bar = Gtk.LevelBar.new_for_interval(0.0, 1.0)
        self._level_bar.add_offset_value(
            Gtk.LEVEL_BAR_OFFSET_LOW, 0.25
        )
        self._level_bar.add_offset_value(
            Gtk.LEVEL_BAR_OFFSET_HIGH, 0.75
        )
        self._level_bar.set_no_show_all(True)
        self._level_bar.hide()

        box.pack_end(self._level_bar, True, True, 0)

        self.connect(
            'clicked', self._on_start_search
        )
        self._cancel_button.connect(
            'clicked', self._on_stop_search
        )
        self.add(box)

    @property
    def progress(self):
        return self._level_bar.get_value()

    @progress.setter
    def progress(self, value):
        self._level_bar.set_value(value)

    def toggle_sensitivity(self, state):
        if state:
            self._on_stop_search(self._cancel_button)
        else:
            self._on_start_search(self)

    def _on_start_search(self, button):
        self.set_sensitive(False)
        self._level_bar.show()
        self._cancel_button.show()
        self._label.set_text('Im doing my best...')

    def _on_stop_search(self, button):
        self.set_sensitive(True)
        self._level_bar.hide()
        self._level_bar.set_value(0)
        self._cancel_button.hide()
        self._label.set_text('Search')


class ContentBox(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)
        self._box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.set_hexpand(True)
        self.set_vexpand(True)
        self._is_empty = True
        self.add(self._box)
        self.make_empty()

    def clear(self):
        for child in self._box.get_children():
            self._box.remove(child)

    def make_empty(self):
        self.clear()

        self._is_empty = True

        da = Gtk.DrawingArea()
        da.connect('draw', self._render_missing)
        self._box.pack_start(da, True, True, 0)

        self.show_all()

    def number_of_children(self):
        return len(list(self._box.get_children())) / 2

    def add_widget(self, widget):
        # Remove the "oh, empty Drawing Area
        if self._is_empty:
            self.clear()
            self._is_empty = False
        self._box.pack_start(widget, False, False, 0)
        self._box.pack_start(templates.DottedSeparator(), False, False, 0)
        self.show_all()

    ##############
    #  Internal  #
    ##############

    def _render_missing(self, widget, ctx):
        alloc = widget.get_allocation()
        w, h = alloc.width, alloc.height

        ctx.set_source_rgba(0, 0, 0, 0.1)
        draw_center_text(ctx, w, h, '⍨', font_size=300)


class MetadataChooser(Gtk.Grid):
    __gsignals__ = {
        'get-type-changed': (
            GObject.SIGNAL_RUN_FIRST,  # Run-Order
            None,                      # Return Type
            (str, )                    # Parameters
        ),
        'toggle-settings': (
            GObject.SIGNAL_RUN_FIRST,  # Run-Order
            None,                      # Return Type
            (bool, )                   # Parameters
        )
    }

    def __init__(self, get_type=None, artist=None, album=None, title=None):
        Gtk.Grid.__init__(self)

        self.set_row_spacing(2)
        self.set_border_width(4)

        self._combo = create_get_type_combobox()
        self._combo.connect('changed', self._on_combo_changed)
        self.attach(self._combo, 0, 0, 1, 1)

        self._cache_counter = 0
        self._entry_map = {}
        self._current_query = None

        def add_entry_row(index, name, icon_name, preset):
            box, entry = make_entry_row(name, icon_name)
            self.attach(box, 0, index, 1, 1)
            self._entry_map[name.lower()] = entry
            if preset is not None:
                entry.set_text(preset)

        add_entry_row(1, 'Artist', 'preferences-desktop-accessibility', artist)
        add_entry_row(2, 'Album', 'media-optical', album)
        add_entry_row(3, 'Title', 'audio-x-generic', title)
        self._combo.emit('changed')

        if get_type is not None:
            for idx, (icon, type_name) in enumerate(self._combo.get_model()):
                if type_name.lower() == get_type.lower():
                    self._combo.set_active(idx)

        self._scale = Gtk.HScale.new_with_range(1, 100, 1)
        self._scale.set_draw_value(False)
        box, self._scale = make_entry_row(
            'Items', 'emblem-downloads', widget=self._scale
        )

        scale_label = Gtk.Label()
        scale_label.set_size_request(15, -1)
        self._scale.connect(
            'value-changed', self._on_scale_value_changed, scale_label
        )
        self._scale.emit('value-changed')
        box.pack_end(scale_label, False, False, 1)
        self.attach(box, 0, 4, 1, 1)

        button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        button_box.get_style_context().add_class(Gtk.STYLE_CLASS_LINKED)

        stop_button = Gtk.Button.new_from_icon_name(
            'gtk-quit', Gtk.IconSize.MENU
        )
        stop_button.set_no_show_all(True)
        stop_button.hide()

        self._search_button = SearchButton(stop_button)
        self._search_button.connect(
            'clicked', lambda *_: self.toggle_sensitivity(False)
        )
        stop_button.connect(
            'clicked', lambda *_: self.toggle_sensitivity(True)
        )
        self._search_button.connect(
            'clicked', self._on_search_results
        )
        stop_button.connect(
            'clicked', self._on_stop_search_results
        )

        settings_button = Gtk.ToggleButton()
        settings_button.set_image(Gtk.Image.new_from_icon_name(
            'gtk-execute', Gtk.IconSize.BUTTON
        ))
        settings_button.get_style_context().add_class(
            Gtk.STYLE_CLASS_SUGGESTED_ACTION
        )
        settings_button.connect(
            'toggled',
            lambda btn: self.emit('toggle-settings', btn.get_active())
        )

        button_box.pack_start(self._search_button, True, True, 0)
        button_box.pack_start(stop_button, False, False, 0)
        button_box.pack_start(settings_button, False, False, 0)

        self.attach(button_box, 0, 5, 1, 1)

        self._content_box = ContentBox()
        self.attach(self._content_box, 0, 6, 1, 1)

        # We can only grab focus when the button actually has a window:
        GLib.idle_add(self._on_search_button_grab_focus)

    def toggle_sensitivity(self, state):
        if state is False:
            # Just make everything insensitive:
            for entry in self._entry_map.values():
                entry.set_sensitive(state)
        else:
            # Check which entries should be sensitive:
            self._combo.emit('changed')

        self._scale.set_sensitive(state)
        self._combo.set_sensitive(state)
        self._search_button.toggle_sensitivity(state)

    def get_selected_type(self):
        iterator = self._combo.get_active_iter()
        if iterator is not None:
            model = self._combo.get_model()
            key = model[iterator][1]
            return key.lower()
        return None

    def show_all_in_database(self, get_type_only=True, amount=-1):
        self._content_box.clear()
        selected_type = self.get_selected_type()
        self._cache_counter = 0

        def _foreach_callback(query, cache):
            if (not get_type_only) or query.get_type == selected_type:
                if amount < 0 or self._cache_counter < amount:
                    template = select_template(query, cache)
                    self._content_box.add_widget(template)
                    self._cache_counter += 1

        g.meta_retriever.database.foreach(_foreach_callback)
        return self._cache_counter

    #############
    #  Signals  #
    #############

    def _on_cache_retrieved(self, order, cache):
        if order.query is not self._current_query:
            return

        template = select_template(order.query, cache)
        self._content_box.add_widget(template)

        amount = order.query.number or 100
        current = self._content_box.number_of_children()
        self._search_button.progress = current / amount

    def _on_search_finished(self, order):
        if order.query is not self._current_query:
            return

        self._search_button.progress = 1.0

        # Uh-oh, nothing found? How embarassing.
        if len(order.results) is 0:
            self._content_box.make_empty()
        else:
            self._content_box.clear()
            for cache in sorted(order.results, key=lambda c: c.rating, reverse=True):
                template = select_template(order.query, cache)
                self._content_box.add_widget(template)

        self.toggle_sensitivity(True)

    def _on_search_results(self, button):
        get_type = self.get_selected_type()
        amount = self._scale.get_value()

        if self._current_query is not None:
            self._current_query.cancel()

        self._current_query = metadata.configure_query(
            get_type, amount=amount,
            artist=self._entry_map['artist'].get_text(),
            album=self._entry_map['album'].get_text(),
            title=self._entry_map['title'].get_text()
        )

        self._content_box.clear()
        self._search_button.progress = 0.1

        g.meta_retriever.push(
            self._current_query,
            self._on_search_finished,
            self._on_cache_retrieved
        )

    def _on_stop_search_results(self, button):
        self._current_query = None

    def _on_scale_value_changed(self, scale, scale_label):
        scale_label.set_text(str(int(scale.get_value())))

    def _on_combo_changed(self, combo):
        selected = self.get_selected_type()
        required = plyr.PROVIDERS.get(selected, {}).get('required')
        if required is not None:
            for key, entry in self._entry_map.items():
                entry.set_sensitive(False)
                entry.set_placeholder_text('<{v} not needed for {t}s>'.format(
                    v=key.capitalize(), t=selected.capitalize()
                ))

            for attr in required:
                entry = self._entry_map[attr]
                entry.set_sensitive(True)
                entry.set_placeholder_text('')

        # Transmit the signal:
        self.emit('get-type-changed', selected)

    def _on_search_button_grab_focus(self):
        self._search_button.set_can_default(True)
        self._search_button.set_can_focus(True)
        self._search_button.grab_default()
        self._search_button.grab_focus()


if __name__ == '__main__':
    # Internal:
    from moosecat.gtk.runner import main
    # from moosecat.metadata import configure_query

    with main(metadata=True) as win:
        chooser = MetadataChooser(
            get_type='cover',
            artist='Akrea',
            album='Lebenslinie'
        )
        win.add(chooser)
        win.set_default_size(450, 650)
