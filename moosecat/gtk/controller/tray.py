from moosecat.boot import g
from moosecat.core import Idle
from moosecat.gtk.interfaces import Hideable
import moosecat.gtk.widgets as widgets

from functools import partial
from gi.repository import Gtk, GLib


class TrayIcon(Hideable):
    def __init__(self, _):
        # buider stays unused
        self._icon = widgets.TrayIcon()
        self._icon.on_popup_func = self._on_popup
        g.client.signal_add('client-event', self._on_client_event)
        GLib.timeout_add(1000, self._on_timeout_event)

        # This is a hack to make GtkStatusIcon look like a widget
        # although it is only a GObject: Patch two functions that do that.
        self._icon.show = partial(self._icon.set_visible, True)
        self._icon.hide = partial(self._icon.set_visible, False)

    def _on_client_event(self, client, event):
        if event & (Idle.PLAYER | Idle.SEEK):
            self._adjust_time()
            with client.lock_status() as status:
                self._adjust_state(status.state)

    def _on_timeout_event(self):
        self._adjust_time()

    def _adjust_time(self):
        self._icon.percent = g.heartbeat.percent

    def _adjust_state(self, state):
        self._icon.state = state

    def _on_popup(self, tray, menu):
        def cm(icon, text, stock_id=None, signal=None):
            menu_item = Gtk.ImageMenuItem(text)
            if signal is not None:
                menu_item.connect('button-press-event', signal)

            if stock_id is not None:
                image = Gtk.Image()
                image.set_from_stock(stock_id, Gtk.IconSize.MENU)
                menu_item.set_image(image)

            menu.append(menu_item)

        sigquit = lambda *_: Gtk.main_quit()
        cm(tray, 'Stop', stock_id='gtk-media-stop', signal=sigquit)
        cm(tray, 'Pause', stock_id='gtk-media-pause', signal=sigquit)
        cm(tray, 'Previous', stock_id='gtk-media-previous', signal=sigquit)
        cm(tray, 'Next', stock_id='gtk-media-next', signal=sigquit)
        menu.append(Gtk.SeparatorMenuItem())
        cm(tray, 'Quit', stock_id='gtk-quit', signal=sigquit)

    ################
    #  Interfaces  #
    ################

    def get_hide_names(self):
        return ['tray']

    def get_hide_widgets(self):
        return [self._icon]
