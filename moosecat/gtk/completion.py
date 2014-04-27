#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import re

# External:
from gi.repository import GLib, GObject, Gtk, Gdk, Pango, GtkSource

# Internal:
from moosecat.gtk.runner import main
from moosecat.core import is_valid_query_tag, query_abbrev_to_full, parse_query, QueryParseException
from moosecat.boot import g


def check_completion(text, value):
    if value.lower().startswith(text):
        return value[len(text):]


class FishySearchEntryImpl(GtkSource.View):
    __gsignals__ = {
        'parse-error': (
            GObject.SIGNAL_RUN_FIRST, None, (int, str)
        ),
        'remove-error': (
            GObject.SIGNAL_RUN_FIRST, None, ()
        ),
        'trigger-search': (
            GObject.SIGNAL_RUN_FIRST, None, ()
        )
    }

    def __init__(self):
        GtkSource.View.__init__(self)
        buf = GtkSource.Buffer()
        buf.set_highlight_matching_brackets(True)
        buf.connect('changed', self.on_text_changed)

        mgr = GtkSource.StyleSchemeManager.get_default()
        style_scheme = mgr.get_scheme('classic')
        if style_scheme:
            buf.set_style_scheme(style_scheme)

        mark_attributes = GtkSource.MarkAttributes()
        mark_attributes.set_icon_name('gtk-warning')
        mark_attributes.set_background(Gdk.RGBA(1, 0, 1, 0.1))
        self.set_mark_attributes('error', mark_attributes, 0)

        self.connect('key-press-event', self.on_key_press_event)
        self.set_border_window_size(Gtk.TextWindowType.TOP, 8)
        self.set_buffer(buf)

        ##############
        #  Text Tags #
        ##############

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

        self._quote_balanced, self._shows_placeholder = True, True
        self._current_tag = self._last_timeout = self._last_trigger = None

        self.set_placeholder_text()

    def clear(self):
        buf = self.get_buffer()
        buf.delete(
            buf.get_start_iter(),
            buf.get_end_iter()
        )
        self._shows_placeholder = False
        buf.set_highlight_matching_brackets(True)

    def set_placeholder_text(self, text=' <Type your query here>'):
        self.clear()
        buf = self.get_buffer()
        self._shows_placeholder = True
        buf.insert_with_tags_by_name(
            self.get_buffer().get_start_iter(),
            text,
            'grey'
        )
        self.emit(
            'move-cursor', Gtk.MovementStep.LOGICAL_POSITIONS,
            -buf.get_property('cursor-position'), False
        )
        buf.set_highlight_matching_brackets(False)

    def get_full_text(self):
        buf = self.get_buffer()
        return buf.get_text(buf.get_start_iter(), buf.get_end_iter(), False)

    def get_query(self):
        buf = self.get_buffer()
        marks = buf.get_mark('l'), buf.get_mark('r')
        if all(marks):
            # We need to filter the suggestion.
            iter_l, iter_r = (buf.get_iter_at_mark(m) for m in marks)
            text_l = buf.get_text(buf.get_start_iter(), iter_r, False)
            text_r = buf.get_text(iter_l, buf.get_end_iter(), False)
            return text_l + text_r
        else:
            return self.get_full_text()

    def get_tag_at_cursor(self):
        right = self.get_buffer().get_property('cursor-position')
        text = self.get_full_text()
        left = text.rfind(' ', 0, right)
        tag = text[left + 1:right]

        if len(tag) is 1:
            tag = query_abbrev_to_full(tag)

        if is_valid_query_tag(tag):
            return tag

    def get_word_at_cursor(self, text):
        right = self.get_buffer().get_property('cursor-position')
        left = -1

        for char in ' :()"' if self._quote_balanced else '":()':
            pos = text.rfind(char, 0, right)
            if pos is not -1 and pos > left:
                left = pos

        # Nothing found, use start.
        left = 0 if left is right else left + 1
        return text[left:right + 1].strip().lower()

    def apply_tag(self, tag, start, end):
        buf = self.get_buffer()
        buf.apply_tag_by_name(
            tag,
            buf.get_iter_at_offset(start),
            buf.get_iter_at_offset(end)
        )

    def apply_colors(self):
        text = self.get_full_text()
        for idx, char in enumerate(text):
            if char in '()':
                self.apply_tag('red', idx, idx + 1)

        for match in self._patterns['qte'].finditer(text):
            self.apply_tag('green', *match.span())

        for match in self._patterns['ops'].finditer(text):
            self.apply_tag('blue', *match.span())

        for match in self._patterns['tag'].finditer(text):
            tag = match.group()[:-1]
            is_tag = is_valid_query_tag(tag) or query_abbrev_to_full(tag)
            self.apply_tag('tag' if is_tag else 'red', *match.span())

    def apply_completion(self, completion):
        if not completion:
            return

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
            return  # Already there.

        try:
            parse_query(text)
        except QueryParseException as err:
            iter_l = buf.get_iter_at_offset(err.pos)
            iter_r = buf.get_iter_at_offset(err.pos)
            iter_l.backward_word_start()
            iter_r.forward_word_end()

            buf.create_source_mark('err_l', 'error', iter_l)
            buf.create_source_mark('err_r', 'error', iter_r)
            buf.apply_tag_by_name('error', iter_l, iter_r)

            self.emit('parse-error', err.pos, err.msg)

    def remove_error(self):
        buf = self.get_buffer()
        marks = buf.get_mark('err_l'), buf.get_mark('err_r')
        if all(marks):
            iter_l, iter_r = (buf.get_iter_at_mark(m) for m in marks)
            buf.remove_tag_by_name('error', iter_l, iter_r)
            for mark in marks:
                buf.delete_mark(mark)

            self.emit('remove-error')

    def defer_completion(self, text):
        if not text:
            return

        if self._last_timeout is not None:
            GLib.source_remove(self._last_timeout)

        timeout = min(300 / len(text), 300)
        self._last_timeout = GLib.timeout_add(
            timeout,
            self.on_do_complete,
            text,
            self.get_buffer().get_property('cursor-position') + 1
        )

    def remove_suggestion(self):
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

    def check_song(self, song, text):
        if self._current_tag is not None:
            test_values = [getattr(song, self._current_tag)]
        else:
            test_values = [song.artist, song.album, song.title]

        for test_value in test_values:
            completion = check_completion(text, test_value)
            if completion is not None:
                return completion

    #############
    #  Signals  #
    #############

    def on_do_complete(self, text, cursor_pos):
        self._last_timeout = None
        if not text:
            return

        buf = self.get_buffer()
        if cursor_pos != buf.get_property('cursor-position'):
            return

        query = text
        if self._current_tag is not None:
            query = '{t}:{v}*'.format(t=self._current_tag, v=text)

        completion = None
        with g.client.store.query(query) as playlist:
            for song in playlist:
                completion = self.check_song(song, text)
                if completion is not None:
                    break

        self.apply_completion(completion)

    def on_text_changed(self, text_buffer):
        # Recolor the whole text new.
        self.apply_colors()

    def on_key_press_event(self, widget, event):
        char = chr(Gdk.keyval_to_unicode(event.keyval))

        if self._shows_placeholder:
            self.clear()

        if char.isprintable() or event.keyval == Gdk.keyval_from_name('BackSpace'):
            self.remove_suggestion()
            self.remove_error()

        if event.keyval == Gdk.keyval_from_name('Return'):
            # Make it impossible to enter a newline.
            # You still can copy one in though.
            self.apply_query_check()
            self.emit('trigger-search')
            return True
        elif event.keyval == Gdk.keyval_from_name('Tab'):
            suggestion = self.remove_suggestion()
            self.get_buffer().insert_at_cursor(
                (suggestion or '') + (self._quote_balanced * ' ')
            )
            return True

        if char is '"':
            self._quote_balanced = not self._quote_balanced
        elif char is ':':
            self._current_tag = self.get_tag_at_cursor()
        elif char.isspace():
            if self._quote_balanced:
                self._current_tag = None
        elif char.isprintable():
            text = self.get_full_text() + char
            word = self.get_word_at_cursor(text)
            self.defer_completion(word)

        if self._last_trigger is not None:
            GLib.source_remove(self._last_trigger)

        self._last_trigger = GLib.timeout_add(
            500, self.on_trigger_search_after_timeout
        )

        # False: Call the default signal handler to inser the char to the View.
        return False

    def on_trigger_search_after_timeout(self):
        self._last_trigger = None
        self.emit('trigger-search')


class FishyEntryIcon(Gtk.EventBox):
    __gtype_name__ = 'FishyEntryIcon'
    __gsignals__ = {
        'clicked': (
            GObject.SIGNAL_RUN_FIRST, None, ()
        )
    }

    def __init__(self, icon_name, bg_color):
        Gtk.EventBox.__init__(self)

        button = Gtk.Button.new_from_icon_name(
            icon_name, Gtk.IconSize.SMALL_TOOLBAR
        )
        button.connect(
            'clicked', lambda widget: self.emit('clicked')
        )

        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(b'''
            FishyEntryIcon {
                background-color: #ffffff;
            }
        ''')

        context = Gtk.StyleContext()
        context.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_USER
        )
        button.set_relief(Gtk.ReliefStyle.NONE)
        button.set_focus_on_click(False)

        self.set_size_request(25, 25)
        self.add(button)

    def get_icon_name(self):
        return self.get_child().get_image().get_property('icon-name')

    def set_icon_name(self, icon_name):
        self.get_child().get_image().set_from_icon_name(
            icon_name, Gtk.IconSize.SMALL_TOOLBAR
        )

    def set_tooltip_markup(self, tooltip):
        child = self.get_child()
        if tooltip is None:
            child.set_has_tooltip(False)
        else:
            child.set_has_tooltip(True)
            child.set_tooltip_markup(tooltip)


class FishySearchEntry(Gtk.Frame):
    __gsignals__ = {
        'search-changed': (
            GObject.SIGNAL_RUN_FIRST, None, (str,)
        )
    }

    def __init__(self):
        Gtk.Frame.__init__(self)

        self._entry = FishySearchEntryImpl()
        self._entry.connect('parse-error', self.on_parse_error)
        self._entry.connect('remove-error', self.on_remove_error)
        self._entry.connect('trigger-search', self.on_emit_search_changed)

        color = Gdk.RGBA(1, 1, 1, 1)
        self._lefty = FishyEntryIcon('gtk-find', color)
        self._right = FishyEntryIcon('window-close', color)
        self._lefty.connect('clicked', self.on_emit_search_changed)
        self._right.connect('clicked', self.on_right_icon_clicked)

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.get_style_context().add_class(Gtk.STYLE_CLASS_LINKED)
        box.pack_start(self._lefty, False, False, 0)
        box.pack_start(Gtk.HSeparator(), False, False, 0)
        box.pack_start(self._entry, True, True, 0)
        box.pack_start(Gtk.HSeparator(), False, False, 0)
        box.pack_start(self._right, False, False, 0)

        align = Gtk.Alignment()
        align.set(0, 0, 1, 0)
        align.add(box)

        self.add(align)

    def on_parse_error(self, entry, pos, msg):
        self._lefty.set_icon_name('dialog-error')
        self._lefty.set_tooltip_markup(
            '<b>Error:</b> {m} (around position {p})'.format(
                p=pos, m=msg
            )
        )

    def on_remove_error(self, entry):
        self._lefty.set_tooltip_markup(None)
        self._lefty.set_icon_name('gtk-find')

    def on_right_icon_clicked(self, icon):
        self._entry.clear()

    def on_emit_search_changed(self, widget):
        query = self._entry.get_query()
        if query:
            self.emit('search-changed', query)


class FishySearchOverlay(Gtk.Overlay):
    def __init__(self):
        Gtk.Overlay.__init__(self)

        self._entry = FishySearchEntry()
        self._entry.set_size_request(300, -1)
        self._entry.set_hexpand(False)
        self._entry.set_vexpand(False)

        self._entry.set_valign(Gtk.Align.END)
        self._entry.set_halign(Gtk.Align.CENTER)

        self.add_overlay(self._entry)


###########################################################################
#                                  Main                                   #
###########################################################################


if __name__ == '__main__':
    entry = FishySearchOverlay()
    entry.add(Gtk.Button('Hi.'))
    with main(store=True, port=6600) as win:
        win.add(entry)
