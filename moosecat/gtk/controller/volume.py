from gi.repository import Gtk, GLib
from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.widgets import BarSlider
from moosecat.boot import g


class Volume(Hideable):
    def __init__(self, builder):
        self._vol_align = builder.get_object('volumeslider_align')
        self._volume_bar = BarSlider()
        self._vol_align.add(self._volume_bar)
