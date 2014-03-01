#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk
from moosecat.boot import g
from moosecat.core import Idle, Status


class PlaybuttonBox(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        # Make the box linked and the corners rounded:
        style_context = self.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        # Receive client events:
        g.client.signal_add('client-event', self._on_client_event)

        # Helper for adding buttons:
        def but(icon, action):
            button = Gtk.Button.new_from_icon_name(icon, Gtk.IconSize.MENU)
            self.pack_start(button, False, False, 0)
            self._actions[button] = action
            button.connect('clicked', self._on_button_clicked)
            return button

        # Add the buttons, order is important here.
        self._actions = {}
        self._prev_button = but('media-skip-backward', g.client.player_previous)
        self._pause_button = but('media-playback-start', g.client.player_pause)
        self._stop_button = but('media-playback-stop', g.client.player_stop)
        self._next_button = but('media-skip-forward', g.client.player_next)

    def _set_button_icon(self, icon):
        self._pause_button.set_image(Gtk.Image.new_from_icon_name(
            icon, Gtk.IconSize.MENU
        ))

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_status() as status:
                if status is None:
                    return
                elif status.state == Status.Playing:
                    self._actions[self._pause_button] = client.player_pause
                    self._set_button_icon('media-playback-pause')
                elif status.state == Status.Paused:
                    self._actions[self._pause_button] = client.player_play
                    self._set_button_icon('media-playback-start')

    def _on_button_clicked(self, button):
        self._actions[button]()


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main() as win:
        win.add(PlaybuttonBox())
