from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk, GLib
from moosecat.boot import g


class TitleLabel(Hideable):
    def __init__(self, builder):
        self._info_label = builder.get_object('top_label')
        g.client.signal_add('client-event', self._on_client_event)

    def _format_text(self, song):
        return '<b>{title}</b> [Track {track}] <small>by</small> {artist} <small>on</small> {album} ({date})'.format(
                title=GLib.markup_escape_text(song.title),
                track=GLib.markup_escape_text(song.track),
                artist=GLib.markup_escape_text(song.artist),
                album=GLib.markup_escape_text(song.album),
                date=GLib.markup_escape_text(song.date)
        ).strip()

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_currentsong() as song:
                if song is not None:
                    text = self._format_text(song)
                    self._info_label.set_markup(text)
                else:
                    self._info_label.set_markup('')
