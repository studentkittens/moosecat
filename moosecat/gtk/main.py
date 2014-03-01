#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, GLib
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application, g
from moosecat.gtk.utils import add_keybindings
import moosecat.gtk.controller as ctrl
from moosecat.core import Idle

import logging

import moosecat.gtk.widgets as widgets


def align_widget(widget):
    """Make a widget use minimal space"""
    alignment = Gtk.Alignment()
    alignment.set(0.5, 0.5, 0, 0)
    alignment.add(widget)
    return alignment


class ProgressSliderControl(widgets.ProgressSlider):
    def __init__(self):
        widgets.ProgressSlider.__init__(self)
        self.set_size_request(250, 11)
        self.connect('percent-change', self._on_percent_change)
        GLib.timeout_add(500, self._timeout_callback)

    # User clicked in the Slider
    def _on_percent_change(self, slider):
        g.client.player_seek_relative(slider.percent)

    def _timeout_callback(self):
        with g.client.lock_status() as status:
            if status is not None:
                total_time = status.total_time
                if total_time is not 0:
                    self.percent = g.heartbeat.elapsed / total_time
        return True


class StarControl(widgets.StarSlider):
    def __init__(self):
        widgets.StarSlider.__init__(self)

        self.set_size_request(self.width_multiplier() * 20, 20)

        # TODO
        # self._star_slider.connect('percent-change', self._on_stars_changed)
        # g.client.signal_add('client-event', self._on_client_event)


class VolumeControl(widgets.BarSlider):
    def __init__(self):
        widgets.BarSlider.__init__(self)
        self.set_size_request(60, 25)

        g.client.signal_add('client-event', self._on_client_event)
        self.connect('percent-change', self._on_click_event)

    def _on_client_event(self, client, event):
        if event & Idle.MIXER:
            with client.lock_status() as status:
                self.percent = status.volume / 100

    def _on_click_event(self, bar):
        with g.client.lock_status() as status:
            if status is not None:
                status.volume = bar.percent * 100


class ActionContainer(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.HORIZONTAL)

        self.pack_start(align_widget(StarControl()), True, True, 0)
        self.pack_start(widgets.MenuButton(), True, True, 0)
        self.show_all()


class MoosecatWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.ApplicationWindow.__init__(self, title="Naglfar", application=app)
        self.set_default_size(1400, 900)

        headerbar = Gtk.HeaderBar()
        headerbar.set_show_close_button(True)
        headerbar.pack_start(widgets.PlaybuttonBox())
        headerbar.pack_start(align_widget(ProgressSliderControl()))
        headerbar.pack_end(align_widget(VolumeControl()))
        headerbar.pack_end(widgets.ModebuttonBox())

        browser_bar = widgets.BrowserBar()
        browser_bar.set_action_widget(ActionContainer())

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        main_box.pack_start(browser_bar, True, True, 0)
        main_box.pack_start(headerbar, False, False, 0)

        self.add(main_box)
        self.show_all()


class MoosecatApplication(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base(verbosity=logging.DEBUG, port=6601, host='localhost')
        boot_metadata()
        boot_store()

    def do_activate(self):
        window = MoosecatWindow(self)
        window.connect('delete-event', self.do_close_application)
        self.add_window(window)
        window.show_all()

        # TODO
        # add_keybindings(window, {
        #     't': lambda win, key: g.gtk_sidebar.toggle_sidebar_visibility()
        # })

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        window.destroy()
        shutdown_application()


if __name__ == '__main__':
    import sys
    sys.exit(MoosecatApplication().run(sys.argv[1:]))
