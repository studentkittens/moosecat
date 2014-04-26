#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import re

# External:
from gi.repository import GLib, Gtk, Gdk, Pango, GtkSource

# Internal:
from moosecat.gtk.runner import main
from moosecat.core import is_valid_query_tag, query_abbrev_to_full
from moosecat.boot import g


class FishySearchEntry(GtkSource.View):
    def __init__(self):
        GtkSource.View.__init__(self)
        buf = GtkSource.Buffer()
        buf.set_highlight_matching_brackets(True)
        buf.connect('changed', self.on_text_changed)

        self.connect('key-press-event', self.on_key_press_event)
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

        # Patterns for Colorization:
        self._patterns = {
            'qte': re.compile('".*?"'),
            'tag': re.compile('\w+:'),
            'ops': re.compile('(\sAND\s|\sOR\s|\sNOT\s|\.\.|\.\.\.|[\*\+!\|])')
        }

        self._quote_balanced = True
        self._current_tag = None
        self._last_timeout = None

    def get_full_text(self):
        buf = self.get_buffer()
        return buf.get_text(buf.get_start_iter(), buf.get_end_iter(), False)

    def get_tag_at_cursor(self):
        right = self.get_buffer().get_property('cursor-position')
        text = self.get_full_text()
        left = text.rfind(' ', 0, right)
        tag = text[left + 1:right]

        print('____TAG: "{}"'.format(tag))
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
        if left is right:
            left = 0
        else:
            left += 1
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

    #############
    #  Signals  #
    #############

    def on_do_complete(self, text, cursor_pos):
        self._last_timeout = None
        if not text:
            return False

        buf = self.get_buffer()
        if cursor_pos != buf.get_property('cursor-position'):
            return False

        completion = None

        if self._current_tag is not None:
            query = '{tag}:{value}*'.format(tag=self._current_tag, value=text)
        else:
            query = text

        def check_completion(text, value):
            if value.lower().startswith(text):
                return value[len(text):]

        value = None
        with g.client.store.query(query) as playlist:
            for song in playlist:
                if self._current_tag is not None:
                    test_value = getattr(song, self._current_tag)
                    value = check_completion(text, test_value)
                else:
                    for test_value in [song.artist, song.album, song.title]:
                        value = check_completion(text, test_value)
                        if value is not None:
                            break
                if value is not None:
                    completion = value
                    break

        if completion:
            iter_cursor = buf.get_iter_at_offset(
                cursor_pos
            )
            iter_suggestion = buf.get_iter_at_offset(
                cursor_pos + len(completion)
            )

            # Remember where we inserted our suggestion:
            buf.create_mark('l', iter_cursor, False)
            buf.create_mark('r', iter_suggestion, True)
            buf.insert_with_tags_by_name(
                iter_cursor, completion, 'grey'
            )

            # Move the cursor back to before the suggestion:
            self.emit(
                'move-cursor', Gtk.MovementStep.LOGICAL_POSITIONS,
                -len(completion), False
            )

        return False

    def on_text_changed(self, text_buffer):
        # Recolor the whole text new.
        self.apply_colors()

    def on_key_press_event(self, widget, event):
        char = chr(Gdk.keyval_to_unicode(event.keyval))
        suggestion = self.remove_suggestion()

        if event.keyval == Gdk.keyval_from_name('Return'):
            # Make it impossible to enter a newline.
            # You still can copy one in though.
            return True
        elif char is '"':
            self._quote_balanced = not self._quote_balanced
            return False
        elif char is ':':
            self._current_tag = self.get_tag_at_cursor()
            return False
        elif event.keyval == Gdk.keyval_from_name('Tab') and suggestion:
            self.get_buffer().insert_at_cursor(suggestion + ' ')
            return True
        elif char.isspace():
            self._current_tag = None
            return False
        elif char.isprintable():
            text = self.get_full_text() + char
            word = self.get_word_at_cursor(text)
            self.defer_completion(word)

        # Allow non-printable characters, but do not process them.
        return False

###########################################################################
#                                  Main                                   #
###########################################################################


if __name__ == '__main__':
    entry = FishySearchEntry()
    with main(store=True, port=6600) as win:
        win.add(entry)
