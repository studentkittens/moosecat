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
            'tag': re.compile('\w+:'),
            'ops': re.compile('(\sAND\s|\sOR\s|\sNOT\s|\.\.|\.\.\.|[\*\+!\|])')
        }

        self._last_timeout = None

    def get_full_text(self):
        buf = self.get_buffer()
        return buf.get_text(buf.get_start_iter(), buf.get_end_iter(), False)

    def apply_tag(self, tag, start, end):
        buf = self.get_buffer()
        buf.apply_tag_by_name(
            tag,
            buf.get_iter_at_offset(start),
            buf.get_iter_at_offset(end)
        )

    def apply_colors(self):
        text = self.get_full_text()
        quote_start = None
        for idx, char in enumerate(text):
            if char in '()':
                self.apply_tag('red', idx, idx + 1)
            elif char is '"':
                if quote_start is None:
                    quote_start = idx
                    continue
                # Color it:
                self.apply_tag('green', quote_start, idx + 1)
                quote_start = None

        for match in self._patterns['ops'].finditer(text):
            self.apply_tag('blue', *match.span())

        for match in self._patterns['tag'].finditer(text):
            tag = match.group()[:-1]
            is_tag = is_valid_query_tag(tag) or query_abbrev_to_full(tag)
            self.apply_tag('tag' if is_tag else 'red', *match.span())

    def defer_completion(self, text):
        if len(text) > 1:
            text = text.strip().lower()
            if self._last_timeout is not None:
                GLib.source_remove(self._last_timeout)

            timeout = min(300 / len(text), 300)
            print(timeout)
            self._last_timeout = GLib.timeout_add(
                timeout,
                self.on_do_complete,
                text
            )

    def remove_suggestion(self):
        buf = self.get_buffer()
        mark_l, mark_r = buf.get_mark('l'), buf.get_mark('r')
        if mark_l and mark_r:
            iter_l = buf.get_iter_at_mark(mark_l)
            iter_r = buf.get_iter_at_mark(mark_r)
            text = buf.get_text(iter_l, iter_r, False)
            buf.delete(iter_l, iter_r)
            buf.delete_mark(mark_l)
            buf.delete_mark(mark_r)
            return text

    #############
    #  Signals  #
    #############

    def on_do_complete(self, text):
        self._last_timeout = None
        if not text:
            return False

        buf = self.get_buffer()
        completion = None
        query = 'artist:{}*'.format(text)

        with g.client.store.query(query, limit_length=1000) as pl:
            for song in pl:
                if song.artist.lower().startswith(text):
                    completion = song.artist[len(text):]
                    break

        if completion:
            cursor_pos = buf.get_property('cursor-position')
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
        buf = self.get_buffer()
        char = chr(Gdk.keyval_to_unicode(event.keyval))

        # TODO: Handle Backspace. Trigger re-eval.

        if event.keyval == Gdk.keyval_from_name('Return'):
            # Make it impossible to enter a newline.
            # You still can copy one in though.
            return True
        elif event.keyval == Gdk.keyval_from_name('BackSpace'):
            self.remove_suggestion()
            return False
        elif event.keyval == Gdk.keyval_from_name('Tab'):
            suggestion = self.remove_suggestion()
            if suggestion is not None:
                buf.insert_at_cursor(suggestion + ' ')
            return True
        elif char.isprintable():
            # TODO: Properly find last word. Not only spaces matter.
            text = self.get_full_text() + char
            cursor_position = buf.get_property('cursor-position')
            fspace_position = text.rfind(' ', 0, cursor_position) + 1
            text = text[fspace_position:cursor_position + 1]

            # Suggest something after some timeout:
            self.defer_completion(text)

            # Delete the old suggestion:
            self.remove_suggestion()
            return False
        else:
            # Allow non-printable characters, but do not process them.
            return False

###########################################################################
#                                  Main                                   #
###########################################################################


if __name__ == '__main__':
    entry = FishySearchEntry()
    with main(store=True, port=6600) as win:
        win.add(entry)
