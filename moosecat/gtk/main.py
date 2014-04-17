#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, GLib
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application, g
from moosecat.gtk.utils import add_keybindings
import moosecat.gtk.controller as ctrl
from moosecat.core import Idle

import logging
import sys

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
        self.set_size_request(200, 11)
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

        # TODO: Functionality for the slider.
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


class TitledHeadbar(Gtk.HeaderBar):
    def __init__(self):
        Gtk.HeaderBar.__init__(self)

        self.pack_start(widgets.PlaybuttonBox())
        self.pack_start(Gtk.VSeparator())
        self.pack_start(align_widget(ProgressSliderControl()))
        self.pack_end(align_widget(VolumeControl()))
        self.pack_end(Gtk.VSeparator())
        self.pack_end(widgets.ModebuttonBox())

        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_currentsong() as song:
                if song is not None:
                    self.set_title('{title}'.format(
                        title=song.title
                    ))
                    self.set_subtitle('{album} - {artist}'.format(
                        album=song.album,
                        artist=song.artist or song.album_artist
                    ))
                elif client.is_connected:
                    self.set_title('Im ready!')
                    self.set_subtitle('Just go ahead and play someting.')
                else:
                    self.set_title('Not connected.')
                    self.set_subtitle('Go select a server.')


class ActionContainer(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.HORIZONTAL)

        self.pack_start(align_widget(StarControl()), True, True, 0)
        self.pack_start(widgets.MenuButton(), True, True, 0)
        self.pack_start(Gtk.VSeparator(), True, True, 1)

        close_button = Gtk.Button.new_from_icon_name(
            'gtk-close', Gtk.IconSize.MENU
        )
        close_button.set_relief(Gtk.ReliefStyle.NONE)
        close_button.connect('clicked', lambda *_: g.gtk_app.quit())

        self.pack_start(close_button, True, True, 1)
        self.show_all()


class MoosecatWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.ApplicationWindow.__init__(self, title="Moosecat", application=app)
        self.set_default_size(1400, 900)

        browser_bar = widgets.BrowserBar()
        browser_bar.set_action_widget(ActionContainer())

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        main_box.pack_start(browser_bar, True, True, 0)
        main_box.pack_start(TitledHeadbar(), False, False, 0)

        g.client.signal_add('client-event', self._on_client_event)

        self.add(main_box)
        self.show_all()

    def _on_client_event(self, client, event):
        with client.lock_currentsong() as song:
            if song is None:
                text = 'Playing: {} - {}'.format(
                    GLib.markup_escape_text(song.artist or song.album_artist),
                    song.title
                )
            else:
                text = 'Not Playing'

            self.set_title('Moosecat ({})'.format(text))


class MoosecatApplication(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # For usage like quit()
        g.register('gtk_app', self)

        # Bring up the core!
        boot_base(verbosity=logging.DEBUG, port=6600, host='localhost')
        boot_metadata()
        boot_store()

    def do_activate(self):
        window = MoosecatWindow(self)
        window.connect('delete-event', self.do_close_application)
        g.register('gtk_main_window', window)
        self.add_window(window)
        window.show_all()

        widgets.TrayIconController()
        # ctrl.TrayIcon('')

        # TODO
        # add_keybindings(window, {
        #     't': lambda win, key: g.gtk_sidebar.toggle_sidebar_visibility()
        # })

    def do_startup(self):
        settings = Gtk.Settings.get_default()
        if '--dark-theme' in sys.argv:
            settings.set_property('gtk-application-prefer-dark-theme', True)

        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        window.destroy()
        shutdown_application()


if __name__ == '__main__':
    import sys
    sys.exit(MoosecatApplication().run(sys.argv[1:]))
