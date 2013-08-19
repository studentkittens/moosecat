from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from moosecat.boot import g

from gi.repository import Gtk, GLib
from time import time


TIMEOUT_MS = 500


class StatusSpinner(Hideable):
    def __init__(self, builder):
        self._spinner = builder.get_object('status_spinner')
        g.client.signal_add('logging', self._on_logging_event)

    def _on_logging_event(self, client, msg, level):
        def _look_if_finished():
            # if time difference greater than one second
            if time() - self._last_update >= (TIMEOUT_MS / 1000):
                # Disable the spinner
                self._spinner.stop()
                self._spinner.hide()
            return False

        self._spinner.start()
        self._spinner.show()
        self._last_update = time()
        GLib.timeout_add(TIMEOUT_MS, _look_if_finished)
