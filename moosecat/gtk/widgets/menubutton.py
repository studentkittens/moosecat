#!/usr/bin/env python
# encoding: utf-8

# Internal:
from moosecat.gtk.widgets import SimplePopupMenu

# External:
from gi.repository import Gtk


class MenuButton(Gtk.MenuButton):
    def __init__(self):
        Gtk.MenuButton.__init__(self)
        self.set_margin_top(2)
        self.set_margin_bottom(4)
        self.set_margin_left(10)
        self.set_margin_right(4)

        menu = SimplePopupMenu()
        menu.simple_add('Connect', 'gtk-connect')
        menu.simple_add_separator()
        menu.simple_add_checkbox('One')
        menu.simple_add_checkbox('Two')
        menu.simple_add_checkbox('Three')
        menu.simple_add_separator()
        menu.simple_add('Quit', 'gtk-quit')

        self.set_halign(Gtk.Align.CENTER)
        self.set_direction(Gtk.ArrowType.DOWN)
        self.set_popup(menu)
        self.set_image(
            Gtk.Image.new_from_icon_name('gtk-execute', Gtk.IconSize.MENU)
        )


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main() as win:
        win.add(MenuButton())
