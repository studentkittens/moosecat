from gi.repository import Gtk, GLib
from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.widgets import ProgressSlider
from moosecat.boot import g

'''
Controls the progression of the Timeslide by forwarding Heartbeat to the View.
'''


class Timeslide(Hideable):
    def __init__(self, builder):
        self._slide_align = builder.get_object('timeslider_align')
        self._slide = ProgressSlider()
        self._slide_align.add(self._slide)
        self._slide.on_percent_change_func = self._on_percent_change

        GLib.timeout_add(500, self._timeout_callback)

    # User clicked in the Slider
    def _on_percent_change(self, slider):
        g.client.player_seek_relative(slider.percent)

    def _timeout_callback(self):
        with g.client.lock_status() as status:
            if status is not None:
                total_time = status.total_time
                if total_time is not 0:
                    self._slide.percent = g.heartbeat.elapsed / total_time
        return True
