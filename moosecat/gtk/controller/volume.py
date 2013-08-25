from gi.repository import Gtk, GLib
from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.widgets import BarSlider
from moosecat.core import Idle
from moosecat.boot import g


class Volume(Hideable):
    def __init__(self, builder):
        self._vol_align = builder.get_object('volumeslider_align')
        self._volume_bar = BarSlider()
        self._vol_align.add(self._volume_bar)

        g.client.signal_add('client-event', self._on_client_event)

        # TODO: Use gsignals like everyone does.
        self._volume_bar.on_percent_change_func = self._on_click_event

    def _on_client_event(self, client, event):
        if event & Idle.MIXER:
            with client.lock_status() as status:
                self._volume_bar.percent = status.volume / 100

    def _on_click_event(self, bar):
        # TODO: This is stupid.
        with g.client.lock_status() as status:
            status.volume = bar.percent * 100
