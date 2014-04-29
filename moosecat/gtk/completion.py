#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import re

# External:
from gi.repository import GLib, GObject, Gtk, Gdk, Pango

# Check if we have access to the GtkSource-Library:
try:
    from gi.repository import GtkSource
    ViewClass, BufferClass = GtkSource.View, GtkSource.Buffer
    GTK_SOURCE_AVAILABLE = True
except ImportError:
    ViewClass, BufferClass = Gtk.TextView, Gtk.TextBuffer
    GTK_SOURCE_AVAILABLE = False

# Internal:
from moosecat.boot import g
from moosecat.gtk.utils import add_keybindings
from moosecat.core import \
    is_valid_query_tag, query_abbrev_to_full, \
    parse_query, QueryParseException


class FishySearchEntryImpl(ViewClass):
    """The Implementation of the autocompletion feature.

    If GtkSource is importable, we use GtkSource.View as parent.
    This enables nice features like paran-matching and highlighted lines.
    """
    __gsignals__ = {
        # Emitted once the user typed a syntactically incorrect query.
        # This might be after enter, pressing search or after 0.5s of no input.
        # Params: The position and description of the error from the Parser.
        'parse-error': (GObject.SIGNAL_RUN_FIRST, None, (int, str)),
        # Emitted after a previous parse-error, if the user starts correcting.
        'remove-error': (GObject.SIGNAL_RUN_FIRST, None, ()),
        # Emitted once the Playlistview should be filtered/searched.
        'trigger-search': (GObject.SIGNAL_RUN_FIRST, None, ())
    }

    def __init__(self):
        ViewClass.__init__(self)

        buf = BufferClass()
        buf.connect('changed', self.on_text_changed)

        if GTK_SOURCE_AVAILABLE:
            # Extra features:
            buf.set_highlight_matching_brackets(True)
            mark_attributes = GtkSource.MarkAttributes()
            mark_attributes.set_icon_name('gtk-warning')
            mark_attributes.set_background(Gdk.RGBA(1, 0, 1, 0.1))
            self.set_mark_attributes('error', mark_attributes, 0)

        self.connect('key-press-event', self.on_key_press_event)
        self.set_buffer(buf)

        # Move the text a bit down, so it's centered. Sadly, hardcoded.
        self.set_border_window_size(Gtk.TextWindowType.TOP, 8)
        self.set_can_focus(True)

        ######################################
        #  Text Tags for Syntax Highlighting #
        ######################################

        buf.create_tag(
            'bold', weight=Pango.Weight.BOLD
        )
        buf.create_tag(
            'grey', foreground_rgba=Gdk.RGBA(0.6, 0.6, 0.6, 1.0)
        )
        buf.create_tag(
            'red', foreground_rgba=Gdk.RGBA(0.5, 0.15, 0.15)
        )
        buf.create_tag(
            'green', foreground_rgba=Gdk.RGBA(0.15, 0.6, 0.15)
        )
        buf.create_tag(
            'blue', foreground_rgba=Gdk.RGBA(0.15, 0.15, 0.6)
        )
        buf.create_tag(
            'tag', foreground_rgba=Gdk.RGBA(0.15, 0.6, 0.6),
            weight=Pango.Weight.BOLD
        )
        buf.create_tag(
            'error', background_rgba=Gdk.RGBA(1.0, 0.7, 0.7)
        )

        # Patterns for Colorization:
        self._patterns = {
            'qte': re.compile('".*?"'),
            'tag': re.compile('\w+:'),
            'ops': re.compile('(\sAND\s|\sOR\s|\sNOT\s|\.\.|\.\.\.|[\*\+!\|])')
        }

        # The current Tag, or None if unused (use artist, album and title then)
        self._current_tag = None

        # Timeout-IDs from GLib.timeout_add
        self._last_timeout = self._last_trigger = None

        # Flags:
        self._quote_balanced, self._shows_placeholder = True, True

        # The Entry is empy by default, show a default placeholder:
        self.set_placeholder_text()

        # Preload the most common indices:
        if hasattr(g, 'client'):
            for tag in ('artist', 'album', 'title'):
                g.client.store.complete(tag, None)

    ######################
    #  Helper Functions  #
    ######################

    def trigger_explicit(self):
        if self._last_trigger:
            GLib.source_remove(self._last_trigger)
            self._last_trigger = None
        self.emit('trigger-search')

    def clear(self):
        """Clear the content of the Search entry."""
        buf = self.get_buffer()
        buf.delete(buf.get_start_iter(), buf.get_end_iter())

        # Enable again:
        self._shows_placeholder = False

    def set_placeholder_text(self, text=' Type your query here'):
        """Show a grey placeholder text.

        Will be removed once something is typed. Text can be anything.
        """
        self.clear()
        self._shows_placeholder = True

        # Inser the text:
        buf = self.get_buffer()
        buf.insert_with_tags_by_name(
            buf.get_start_iter(), text, 'grey'
        )

        # Move cursor back to the beginning:
        self.emit(
            'move-cursor', Gtk.MovementStep.LOGICAL_POSITIONS,
            -buf.get_property('cursor-position'), False
        )

    def get_full_text(self):
        """Get the full, shown text.

        Warning: This includes the currently shown suggestion.
                 Use get_query() if unwanted.
        """
        buf = self.get_buffer()
        return buf.get_text(buf.get_start_iter(), buf.get_end_iter(), False)

    def get_query(self):
        """Get the current query, this excludes any greyed out text."""
        buf = self.get_buffer()
        marks = buf.get_mark('l'), buf.get_mark('r')

        if all(marks):
            # We need to filter the suggestion.
            iter_l, iter_r = (buf.get_iter_at_mark(m) for m in marks)
            text_l = buf.get_text(buf.get_start_iter(), iter_r, False)
            text_r = buf.get_text(iter_l, buf.get_end_iter(), False)
            return (text_l + text_r).lower().strip()

        # We can simply use the full text:
        return self.get_full_text().lower().strip()

    def get_tag_at_cursor(self):
        """Called when encountering a colon.

        Finds out which tag was entered and validates it.
        """
        right = self.get_buffer().get_property('cursor-position')
        text = self.get_full_text()
        left = text.rfind(' ', 0, right)
        tag = text[left + 1:right]

        # Abrreviated tags need to be converted to the full form.
        if len(tag) is 1:
            tag = query_abbrev_to_full(tag)

        # Only process valid tags.
        if is_valid_query_tag(tag):
            return tag

    def get_word_at_cursor(self, text):
        """Find out the last word, used as base for the suggestion."""
        left, right = -1, self.get_buffer().get_property('cursor-position')

        # Find the nearest pos of a word delimiter:
        for char in ' :()"' if self._quote_balanced else '":()':
            pos = text.rfind(char, 0, right)
            if pos is not -1 and pos > left:
                left = pos

        # Nothing found, use start.
        left = 0 if left is right else left + 1

        # Assemble!
        return text[left:right + 1].strip().lower()

    def apply_tag(self, tag, start, end):
        """Apply a tag between the iters start and end."""
        buf = self.get_buffer()
        buf.apply_tag_by_name(
            tag,
            buf.get_iter_at_offset(start),
            buf.get_iter_at_offset(end)
        )

    def apply_colors(self):
        """Called once the buffer contents are changed.

        Live-highlighting would be possible, but would be tedious to implement.
        """
        # Color the parantheses red:
        text = self.get_full_text()
        for idx, char in enumerate(text):
            if char in '()':
                self.apply_tag('red', idx, idx + 1)

        # Color the text between quotes green:
        for match in self._patterns['qte'].finditer(text):
            self.apply_tag('green', *match.span())

        # Color all operators slightly blueish:
        for match in self._patterns['ops'].finditer(text):
            self.apply_tag('blue', *match.span())

        # Color all valid tags turquoise, invalid will get red:
        for match in self._patterns['tag'].finditer(text):
            tag = match.group()[:-1]
            is_tag = is_valid_query_tag(tag) or query_abbrev_to_full(tag)
            self.apply_tag('tag' if is_tag else 'red', *match.span())

    def apply_completion(self, completion):
        """Show the completion as greyed out text (handles empty text)
        """
        if not completion:
            return

        # Find out the insert position:
        buf = self.get_buffer()
        cursor_pos = buf.get_property('cursor-position')
        iter_cursor = buf.get_iter_at_offset(cursor_pos)
        iter_suggestion = buf.get_iter_at_offset(cursor_pos + len(completion))

        # Remember where we inserted our suggestion:
        buf.create_mark('l', iter_cursor, False)
        buf.create_mark('r', iter_suggestion, True)
        buf.insert_with_tags_by_name(iter_cursor, completion, 'grey')

        # Move the cursor back to before the suggestion:
        self.emit(
            'move-cursor', Gtk.MovementStep.LOGICAL_POSITIONS,
            -len(completion), False
        )

    def apply_query_check(self):
        buf = self.get_buffer()
        text = self.get_full_text()
        marks = buf.get_mark('err_l'), buf.get_mark('err_r')
        if all(marks):
            return False  # Already there, the error is already highlighted.

        try:
            parse_query(text)
            return True
        except QueryParseException as err:
            iter_l = buf.get_iter_at_offset(err.pos)
            iter_r = buf.get_iter_at_offset(err.pos)
            iter_l.backward_word_start()
            iter_r.forward_word_end()

            if GTK_SOURCE_AVAILABLE:
                buf.create_source_mark('err_l', 'error', iter_l)
                buf.create_source_mark('err_r', 'error', iter_r)
            else:
                buf.create_mark('err_l', iter_l, False)
                buf.create_mark('err_r', iter_r, False)

            buf.apply_tag_by_name('error', iter_l, iter_r)

            self.emit('parse-error', err.pos, err.msg)
            return False

    def remove_error(self):
        """Remove all shown error markup or do nothing."""
        buf = self.get_buffer()
        marks = buf.get_mark('err_l'), buf.get_mark('err_r')
        if all(marks):
            iter_l, iter_r = (buf.get_iter_at_mark(m) for m in marks)
            buf.remove_tag_by_name('error', iter_l, iter_r)
            for mark in marks:
                buf.delete_mark(mark)

            self.emit('remove-error')

    def defer_completion(self, text):
        """Schedule a timeout event to complete the current text"""
        # Remove the previously active completion.
        if self._last_timeout is not None:
            GLib.source_remove(self._last_timeout)

        # Completing empty text does not make sense:
        if not text:
            return

        # Calculate the timeout. 300 is the max. 100 the min.
        self._last_timeout = GLib.timeout_add(
            max(min(400 / len(text), 400), 100),
            self.on_do_complete,
            text,
            self.get_buffer().get_property('cursor-position') + 1
        )

    def remove_suggestion(self):
        """Remove the previously applied completion (remove all grey text)"""
        buf = self.get_buffer()
        marks = buf.get_mark('l'), buf.get_mark('r')
        if not any(marks):
            return None

        iter_l, iter_r = (buf.get_iter_at_mark(m) for m in marks)
        text = buf.get_text(iter_l, iter_r, False)
        buf.delete(iter_l, iter_r)
        for mark in marks:
            buf.delete_mark(mark)

        return text

    #####################
    #  Signal Handlers  #
    #####################

    def on_do_complete(self, text, cursor_pos):
        """Called as a timeout event, once a suggestion is wanted.

        The text-base and the cursor_pos where the completion is desired,
        is passed.
        """
        self._last_timeout = None
        if not text:
            return

        # Check if the cursor moved in the meantime.
        # Can happen in corner-cases. Just return in this case.
        buf = self.get_buffer()
        if cursor_pos != buf.get_property('cursor-position'):
            return

        # Build the query:
        if self._current_tag is not None:
            test_values = [self._current_tag]
        else:
            test_values = ('artist', 'album', 'title')

        for test_value in test_values:
            completion = g.client.store.complete(test_value, text)
            if completion is not None:
                completion = completion[len(text):]
                break

        # If non-empty, show the completion.
        self.apply_completion(completion)

    def on_text_changed(self, text_buffer):
        """Called once a visual change is made to the buffer"""
        # Recolor the whole text new.
        self.apply_colors()

    def on_trigger_search_after_timeout(self):
        """Called on Enter or Timeout. Emits a playlist-filter signal."""
        self._last_trigger = None
        self.emit('trigger-search')

    def on_key_press_event(self, widget, event):
        """Called on every keystroke passed to the entry. Mainlogic here."""
        char = chr(Gdk.keyval_to_unicode(event.keyval))

        if self._shows_placeholder:
            self.clear()

        # Remove any previous suggestion, since it is/might be invalid now.
        if char.isprintable() or event.keyval == Gdk.keyval_from_name('BackSpace'):
            self.remove_suggestion()
            self.remove_error()

        if event.keyval == Gdk.keyval_from_name('BackSpace'):
            if len(self.get_full_text()) is 1:
                self.trigger_explicit()
                return False

        if event.keyval == Gdk.keyval_from_name('Return'):
            # Make it impossible to enter a newline.
            # You still can copy one in though.
            self.apply_query_check()
            self.trigger_explicit()
            return True
        elif event.keyval == Gdk.keyval_from_name('Tab'):
            # Apply the suggestion.
            suggestion = self.remove_suggestion()
            if suggestion:
                self.get_buffer().insert_at_cursor(
                    suggestion + (self._quote_balanced * ' ')
                )
            self.trigger_explicit()
            return True  # Filter the Tab.

        # Order matters here.
        if char is '"':
            # Remember if the quote-count is even (= true)
            self._quote_balanced = not self._quote_balanced

            # When typing a closing ", the current tag will be invalid.
            if self._quote_balanced:
                self._current_tag = None
        elif char is ':':
            # Find out what tag was typed and if it's valid:
            self._current_tag = self.get_tag_at_cursor()
        elif char.isspace():
            # If we aren't in a quote block, invalidate the current tag.
            if self._quote_balanced:
                self._current_tag = None
        elif char.isprintable():
            # Find out the current text and schedule a completion.
            text = self.get_full_text() + char
            word = self.get_word_at_cursor(text)
            self.defer_completion(word)

        # Remove the last trigger timeout. After 500s the Playlist if filtered.
        if self._last_trigger is not None:
            GLib.source_remove(self._last_trigger)

        self._last_trigger = GLib.timeout_add(
            max(min(1000 / (len(self.get_query()) or 1), 1000), 400),
            self.on_trigger_search_after_timeout
        )

        # False: Call the default signal handler to inser the char to the View.
        return False

###########################################################################
#                            Container Widgets                            #
###########################################################################


def get_entry_bg_color(flag=Gtk.StateFlags.NORMAL, use_hex=True):
    "Get the background color of an Gtk.Entry (hex). We want the same color."
    entry = Gtk.Entry()
    rgb = entry.get_style_context().get_background_color(flag)
    if use_hex:
        return '#{:02x}{:02x}{:02x}'.format(
            int(0xff * rgb.red),
            int(0xff * rgb.green),
            int(0xff * rgb.blue)
        )
    return rgb


class FishyEntryIcon(Gtk.EventBox):
    """Icons around the Searchentry.  """
    __gtype_name__ = 'FishyEntryIcon'
    __gsignals__ = {
        'clicked': (GObject.SIGNAL_RUN_FIRST, None, ())
    }

    def __init__(self, icon_name):
        Gtk.EventBox.__init__(self)
        button = Gtk.Button.new_from_icon_name(
            icon_name, Gtk.IconSize.SMALL_TOOLBAR
        )

        # Forward the clicked-signal.
        button.connect('clicked', lambda widget: self.emit('clicked'))
        button.set_can_focus(False)

        # Fix the background color.
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data("""
            FishyEntryIcon {{
                background-color: {};
            }}
        """.format(get_entry_bg_color()).encode('utf-8'))

        # Add the css and fix the button appearance.
        context = Gtk.StyleContext()
        context.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_USER
        )
        button.set_relief(Gtk.ReliefStyle.NONE)
        button.set_focus_on_click(False)

        # Hardcoded, to look okay.
        self.set_size_request(25, 25)
        self.add(button)

    def set_icon_name(self, icon_name):
        """Set the icon as icon-name."""
        self.get_child().get_image().set_from_icon_name(
            icon_name, Gtk.IconSize.SMALL_TOOLBAR
        )

    def set_tooltip_markup(self, tooltip):
        """Set the tooltip (for errors) or None to disable the tooltip"""
        child = self.get_child()
        if tooltip is None:
            child.set_has_tooltip(False)
        else:
            child.set_has_tooltip(True)
            child.set_tooltip_markup(tooltip)


class FishySearchSeparator(Gtk.DrawingArea):
    """Custom Separator that does not have an transparent background.

    Gtk.Separator is not usable with Gtk.Overlay you can see a few pixel
    through it.
    """
    def __init__(self, horizontal=True):
        Gtk.DrawingArea.__init__(self)
        self._bg = get_entry_bg_color(flag=Gtk.StateFlags.ACTIVE, use_hex=False)

        if horizontal:
            self.set_size_request(1, -1)
        else:
            self.set_size_request(-1, 1)

        self.connect('draw', self.on_draw)

    def on_draw(self, da, ctx):
        ctx.set_source_rgb(self._bg.red, self._bg.green, self._bg.blue)
        ctx.paint()


class FishySearchEntry(Gtk.Frame):
    """The actual completion-entry widget. Use this in the application.

    This widget is a a container around FishySearchEntryImpl.
    """
    # Triggered once a playlist search is desired.
    __gsignals__ = {
        'search-changed': (GObject.SIGNAL_RUN_FIRST, None, (str,))
    }

    def __init__(self):
        Gtk.Frame.__init__(self)

        self._entry = FishySearchEntryImpl()
        self._entry.connect('parse-error', self.on_parse_error)
        self._entry.connect('remove-error', self.on_remove_error)
        self._entry.connect('trigger-search', self.on_emit_search_changed)

        self._lefty = FishyEntryIcon('gtk-find')
        self._right = FishyEntryIcon('edit-delete')
        self._lefty.connect('clicked', self.on_emit_search_changed)
        self._right.connect('clicked', self.on_right_icon_clicked)

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(self._lefty, False, False, 0)
        box.pack_start(FishySearchSeparator(), False, False, 0)
        box.pack_start(self._entry, True, True, 0)
        box.pack_start(FishySearchSeparator(), False, False, 0)
        box.pack_start(self._right, False, False, 0)

        self._last_query = ''
        self._full_query = None

        align = Gtk.Alignment.new(0, 0, 1, 0)
        align.add(box)
        self.add(align)

    def get_widget(self):
        """Returns the TextView widget that should receive focus"""
        return self._entry

    def on_parse_error(self, entry, pos, msg):
        """Show an error and a tooltip"""
        self._lefty.set_icon_name('dialog-error')
        self._lefty.set_tooltip_markup(
            '<b>Error:</b> {m} (around position {p})'.format(p=pos, m=msg)
        )

    def on_remove_error(self, entry):
        """User typed something which might have fixed the error."""
        self._lefty.set_tooltip_markup(None)
        self._lefty.set_icon_name('gtk-find')

    def on_full_refresh(self):
        self._last_query = '*'
        self._full_query = None
        self.emit('search-changed', '*')

    def on_right_icon_clicked(self, icon):
        """Clear the entry."""
        self._entry.clear()
        if self._last_query != '*':
            GLib.timeout_add(10, lambda: self.emit('search-changed', '*'))

    def on_emit_search_changed(self, widget):
        """Forward the playlist-filter signal"""
        query = self._entry.get_query()

        # We have a new query, cancel the full refresh.
        if self._full_query:
            GLib.source_remove(self._full_query)
            self._full_query = None

        if not query:
            # Schedule a full refresh and hope it never happens.
            GLib.timeout_add(666, self.on_full_refresh)
            return

        if self._last_query == query:
            return

        if not self._entry.apply_query_check():
            return

        self._last_query = query
        self.emit('search-changed', query)

###########################################################################
#                             Utility Widgets                             #
###########################################################################


class FishySearchOverlay(Gtk.Overlay):
    """Utility widget.

    A overlay, where the the searchentry is centered at the bottom.
    The widget might be hidden or shown with a revealer-animation.
    """
    def __init__(self):
        Gtk.Overlay.__init__(self)

        self._revealer = Gtk.Revealer()
        self._revealer.set_size_request(400, -1)
        self._revealer.set_hexpand(False)
        self._revealer.set_vexpand(False)
        self._revealer.set_valign(Gtk.Align.END)
        self._revealer.set_halign(Gtk.Align.CENTER)

        entry = FishySearchEntry()
        self._revealer.add(entry)
        self._revealer.set_transition_duration(666)
        self._revealer.set_transition_type(
            Gtk.RevealerTransitionType.CROSSFADE
        )
        self.add_overlay(self._revealer)
        self.connect('size-allocate', self.on_resize)
        self.hide()

        add_keybindings(entry.get_widget(), {
            '<Ctrl>e': lambda view, key: self.hide()
        })

    def on_resize(self, widget, alloc):
        # Handle smaller sizes than 400 correctly:
        if alloc.width < 400:
            self._revealer.set_size_request(alloc.width, -1)
        else:
            self._revealer.set_size_request(400, -1)

    def add(self, child):
        add_keybindings(child, {
            '<Ctrl>e': lambda view, key: self.show(),
            '/': lambda view, key: self.show(),
        })

        Gtk.Overlay.add(self, child)

    def hide(self):
        self._revealer.set_reveal_child(False)
        if self.get_child():
            self.get_child().grab_focus()

        return True

    def show(self):
        self._revealer.set_reveal_child(True)
        self._revealer.get_child().grab_focus()
        return True

    def get_entry(self):
        return self._revealer.get_child()

###########################################################################
#                                  Main                                   #
###########################################################################


if __name__ == '__main__':
    from moosecat.gtk.runner import main
    from moosecat.gtk.widgets import BrowserBar
    from moosecat.gtk.browser.queue import QueuePlaylistWidget

    def search_changed(entry, query, playlist_widget):
        playlist_widget.do_search(query)

    with main(store=True, port=6600) as win:
        playlist_widget = QueuePlaylistWidget()
        overlay = FishySearchOverlay()
        overlay.get_entry().connect(
            'search-changed', search_changed, playlist_widget
        )
        overlay.add(playlist_widget)
        win.add(overlay)
        win.set_size_request(1000, 500)
        GLib.timeout_add(500, lambda: overlay.show())
