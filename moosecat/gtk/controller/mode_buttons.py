from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk
from moosecat.boot import g


class ModeButtons(Hideable):
    def __init__(self, builder):
        go = builder.get_object
        self._actions = {
            'icon_single': Status.single,
            'icon_repeat': Status.repeat,
            'icon_random': Status.random,
            'icon_consume': Status.consume
        }
        self._button_names = list(self._actions.keys())
        self._buttons = [go(name) for name in self._button_names]
        self._single, self._repeat, self._consume, self._random = self._buttons

        for button in self._buttons:
            button.connect('clicked', self._on_button_clicked)
        g.client.signal_add('client-event', self._on_client_event)

    def _on_button_clicked(self, button):
        # Get the corresponding action for this button
        name = self._button_names[self._buttons.index(button)]
        action_func = self._actions[name]

        # Trigger the action
        with g.client.lock_status() as status:
            action_func.__set__(status, button.get_active())

    def _on_client_event(self, client, event):
        if event & Idle.OPTIONS:
            # Update appearance according to MPD's state
            with g.client.lock_status() as status:
                self._single.set_active(status.single)
                self._repeat.set_active(status.repeat)
                self._consume.set_active(status.consume)
                self._random.set_active(status.random)
