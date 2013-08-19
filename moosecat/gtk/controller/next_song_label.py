from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk, GLib
from moosecat.boot import g


def _format_label_text(song):
    if song:
        return '<small><b>Next: </b>{artist} - {title}</small>'.format(
                artist=GLib.markup_escape_text(song.artist),
                title=GLib.markup_escape_text(song.title)
        )
    else:
        return '<small><b>Next: </b><small>(( none ))</small></small>'


class NextSongLabel(Hideable):
    def __init__(self, builder):
        self._label = builder.get_object('next_song_label')
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_nextsong() as song:
                self._label.set_markup(_format_label_text(song))
