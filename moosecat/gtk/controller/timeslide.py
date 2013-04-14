from gi.repository import Gtk, GLib
from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.widgets import ProgressSlider
from moosecat.boot import g


class Timeslide(Hideable):
    def __init__(self, builder):
        self._slide_align = builder.get_object('timeslider_align')
        self._slide = ProgressSlider()
        self._slide_align.add(self._slide)

        GLib.timeout_add(500, self._timeout_callback)

    def _timeout_callback(self):
        total_time = g.client.status.total_time
        if total_time is not 0:
            self._slide.percent = g.heartbeat.elapsed / total_time

        return True
