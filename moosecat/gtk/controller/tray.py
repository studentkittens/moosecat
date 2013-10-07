from moosecat.boot import g
from moosecat.core import Idle
from moosecat.gtk.interfaces import Hideable
import moosecat.gtk.widgets as widgets

from functools import partial
from gi.repository import Gtk, GLib


class TrayIcon(Hideable):
    def __init__(self, _):  # buider stays unused
        self._icon = widgets.TrayIcon()
        self._icon.connect('popup-menu', self._on_popup)
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
        self._create_menu()

    ######################
    #  Helper Functions  #
    ######################

    def _adjust_time(self):
        with g.client.lock_status() as status:
            if status is not None:
                total_time = status.total_time
                if total_time is 0:
                    self._icon_percent = 0
                else:
                    self._icon.percent = (g.heartbeat.elapsed / total_time) * 100

    def _adjust_state(self, state):
        self._icon.state = state

    def _create_menu(self):
        # Note that the visual appearance is already similar to the final menu.
        # :)
        self._menu = widgets.SimplePopupMenu()
        self._menu.simple_add(
                'Show Window',
                callback=lambda *_: g.gtk_app.activate(),
                stock_id='gtk-execute'
        )
        self._menu.simple_add(
                'Preferences',
                callback=lambda *_: g.gtk_sidebar.switch_to('Settings'),
                stock_id='gtk-preferences'
        )
        # ------------------------------
        self._menu.simple_add_separator()
        self._menu.simple_add(
                'Pause',
                callback=lambda *_: g.client.player_pause(),
                stock_id='gtk-media-pause'
        )
        self._menu.simple_add(
                'Previous',
                callback=lambda *_: g.client.player_previous(),
                stock_id='gtk-media-previous'
        )
        self._menu.simple_add(
                'Next',
                callback=lambda *_: g.client.player_next(),
                stock_id='gtk-media-next'
        )
        self._menu.simple_add(
                'Stop',
                callback=lambda *_: g.client.player_stop(),
                stock_id='gtk-media-stop'
        )
        # ------------------------------
        self._menu.simple_add_separator()
        self._menu.simple_add(
                'Quit',
                callback=lambda *_: g.gtk_app.quit(),
                stock_id='gtk-media-quit'
        )

    ######################
    #  Signal Callbacks  #
    ######################

    def _on_client_event(self, client, event):
        self._adjust_time()
        with client.lock_status() as status:
            self._adjust_state(status.state)

    def _on_timeout_event(self):
        self._adjust_time()

    def _on_popup(self, icon, button, time):
        self._menu.popup(None, None, None, None, button, time)

    ################
    #  Interfaces  #
    ################

    def get_hide_names(self):
        return ['tray']

    def get_hide_widgets(self):
        return [self._icon]
