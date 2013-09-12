from moosecat.boot import g
from moosecat.core import Idle
from moosecat.gtk.interfaces import Hideable
import moosecat.gtk.widgets as widgets

from functools import partial
from gi.repository import Gtk, GLib


class TrayIcon(Hideable):
    def __init__(self, _):  # buider stays unused
        self._icon = widgets.TrayIcon()
        self._icon.on_popup_func = self._on_popup
        g.client.signal_add(
                'client-event',
                self._on_client_event,
                mask=(Idle.PLAYER | Idle.SEEK)
        )
        GLib.timeout_add(1000, self._on_timeout_event)

        # This is a hack to make GtkStatusIcon look like a widget
        # although it is only a GObject: Monkey-Patch two functions that do that.
        self._icon.show = partial(self._icon.set_visible, True)
        self._icon.hide = partial(self._icon.set_visible, False)

    def _on_client_event(self, client, event):
        self._adjust_time()
        with client.lock_status() as status:
            self._adjust_state(status.state)

    def _on_timeout_event(self):
        self._adjust_time()

    def _adjust_time(self):
        with g.client.lock_status() as status:
            if status is not None:
                total_time = status.total_time
                self._icon.percent = (g.heartbeat.elapsed / total_time) * 100

    def _adjust_state(self, state):
        self._icon.state = state

    def _on_popup(self, tray, menu):
        def create_menu(icon, text, stock_id=None, signal=None):
            menu_item = Gtk.ImageMenuItem(text)
            if signal is not None:
                menu_item.connect('button-press-event', signal)
            if stock_id is not None:
                image = Gtk.Image()
                image.set_from_stock(stock_id, Gtk.IconSize.MENU)
                menu_item.set_image(image)
            menu.append(menu_item)

        def create_separator():
            menu.append(Gtk.SeparatorMenuItem())

        create_menu(
                tray, 'Show Window', stock_id='gtk-execute',
                signal=lambda *_: g.gtk_app.activate()
        )

        create_menu(
                tray, 'Preferences', stock_id='gtk-preferences',
                signal=lambda *_: g.gtk_sidebar.switch_to('Settings')
        )
        create_separator()
        create_menu(
                tray, 'Stop', stock_id='gtk-media-stop',
                signal=lambda *_: g.gtk_app.player_stop()
        )
        create_menu(
                tray, 'Pause', stock_id='gtk-media-pause',
                signal=lambda *_: g.client.player_pause())
        create_menu(
                tray, 'Previous', stock_id='gtk-media-previous',
                signal=lambda *_: g.client.player_previous())
        create_menu(
                tray, 'Next', stock_id='gtk-media-next',
                signal=lambda *_: g.client.player_next())
        create_separator()
        create_menu(
                tray, 'Quit', stock_id='gtk-quit',
                signal=lambda *_: g.gtk_app.quit()
        )

    ################
    #  Interfaces  #
    ################

    def get_hide_names(self):
        return ['tray']

    def get_hide_widgets(self):
        return [self._icon]
