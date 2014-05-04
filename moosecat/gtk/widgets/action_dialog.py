#!/usr/bin/env python
# encoding: utf-8

from gi.repository import GObject, Gtk


WINDOW = Gtk.Window()


def create_action_popover(relative, content, title, subtitle=None):
    popover = Gtk.Popover.new(relative)
    popover.set_relative_to(relative)
    popover.set_modal(True)
    popover.set_position(Gtk.PositionType.BOTTOM)

    btn_l = Gtk.Button('Cancel')
    btn_r = Gtk.Button('Accept')
    btn_l.set_can_focus(False)
    btn_r.set_can_focus(False)
    btn_r.get_style_context().add_class(Gtk.STYLE_CLASS_SUGGESTED_ACTION)

    header_bar = Gtk.HeaderBar()
    header_bar.pack_start(btn_l)
    header_bar.pack_end(btn_r)
    header_bar.set_title(title)
    if subtitle is not None:
        header_bar.set_subtitle(subtitle)

    content.set_hexpand(True)
    content.set_vexpand(True)
    content.set_halign(Gtk.Align.FILL)
    content.set_valign(Gtk.Align.FILL)

    grid = Gtk.Grid()
    grid.attach(header_bar, 0, 0, 1, 1)
    grid.attach(content, 0, 1, 1, 1)
    # grid.set_border_width(15)
    popover.add(grid)
    grid.show_all()

    return popover


if __name__ == '__main__':
    b = Gtk.Button('Click me for a cool popup!')
    b.set_border_width(200)

    levelbar = Gtk.LevelBar.new_for_interval(0.0, 1.0)
    levelbar.set_size_request(95, -1)
    label = Gtk.Label()

    levelbar.add_offset_value(Gtk.LEVEL_BAR_OFFSET_LOW, 0.33)
    levelbar.add_offset_value(Gtk.LEVEL_BAR_OFFSET_HIGH, 0.66)
    levelbar.set_property('margin', 20)
    levelbar.set_value(0.88)

    def show(*_):
        popover = create_action_popover(
            b,
            levelbar,
            'Configure Column',
            'Slide stuff around until it works.'
        )
        popover.show_all()

    b.connect('clicked', show)
    WINDOW.connect('destroy', Gtk.main_quit)
    WINDOW.add(b)
    WINDOW.show_all()

    Gtk.main()
