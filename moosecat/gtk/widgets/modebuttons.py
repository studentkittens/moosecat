#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk
from moosecat.boot import g
from moosecat.core import Idle, Status


def toggle_button_from_icon_name(icon_name, icon_size):
    """Helper Function that does the same as Gtk.Button.new_from_icon_name.

    But instead for Gtk.ToggleButton. Why ever there is no pendant.
    """
    toggler = Gtk.ToggleButton()
    toggler.set_image(Gtk.Image.new_from_icon_name(
        icon_name, icon_size
    ))
    return toggler


class ModebuttonBox(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        style_context = self.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        g.client.signal_add('client-event', self._on_client_event)

        # Helper for adding buttons:
        def but(icon, action):
            button = toggle_button_from_icon_name(icon, Gtk.IconSize.MENU)
            self.pack_start(button, False, False, 0)
            self._actions[button] = action
            button.connect('clicked', self._on_button_clicked)
            return button

        self._actions = {}
        self._single_button = but('object-rotate-left', 'single')
        self._repeat_button = but('media-playlist-repeat', 'repeat')
        self._random_button = but('media-playlist-shuffle', 'random')
        self._consume_button = but('media-tape', 'consume')

    def _on_button_clicked(self, button):
        # Get the corresponding action for this button
        property_name = self._actions[button]

        # Trigger the action
        with g.client.lock_status() as status:
            is_active = button.get_active()
            current_state = getattr(status, property_name)
            if is_active != current_state:
                setattr(status, property_name, is_active)

    def _on_client_event(self, client, event):
        if event & Idle.OPTIONS:
            # Update appearance according to MPD's state
            with g.client.lock_status() as status:
                for button, property_name in self._actions.items():
                    button.set_active(getattr(status, property_name))


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main() as win:
        win.add(ModebuttonBox())
