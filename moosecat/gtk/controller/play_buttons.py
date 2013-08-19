from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk
from moosecat.boot import g


class PlayButtons(Hideable):
    def __init__(self, builder):
        self._actions = {
                'stop_button': g.client.player_stop,
                'pause_button': g.client.player_pause,
                'prev_button': g.client.player_previous,
                'next_button': g.client.player_next
        }

        self._buttons = [builder.get_object(button) for button in self._actions.keys()]
        for button in self._buttons:
            button.connect('clicked', self._on_button_clicked)
        self._pause_image = builder.get_object('pause_image')
        g.client.signal_add('client-event', self._on_client_event)

    def _set_button_icon(self, icon):
        self._pause_image.set_from_stock(icon, Gtk.IconSize.SMALL_TOOLBAR)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_status() as status:
                if status.state == Status.Playing:
                    self._actions['pause_button'] = client.player_pause
                    self._set_button_icon('gtk-media-pause')
                elif status.state == Status.Paused:
                    self._actions['pause_button'] = client.player_play
                    self._set_button_icon('gtk-media-play')

    def _on_button_clicked(self, button):
        keys = list(self._actions.keys())
        aidx = self._buttons.index(button)
        if 0 <= aidx < len(keys):
            action = self._actions[keys[aidx]]
            action()
