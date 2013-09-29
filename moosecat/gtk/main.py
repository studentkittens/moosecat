#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application, g
import moosecat.gtk.controller as ctrl

import logging
import os


def glade_path(glade_name):
    script_path = os.path.dirname(os.path.realpath(__file__))
    return os.path.join(script_path, 'ui', glade_name + '.glade')


class Application(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base(verbosity=logging.DEBUG)
        boot_metadata()
        boot_store()

        # Bind a global reference to gtk_builder
        builder = Gtk.Builder()
        builder.add_from_file(glade_path('main'))
        g.register('gtk_builder', builder)

        # Bind a global reference to the Application Instance
        g.register('gtk_app', self)

        # Sidebar is explictly instanced, since we need the reference to it.
        g.register('gtk_sidebar', ctrl.Sidebar(builder))

        controller_class_list = (
            ctrl.Timeslide, ctrl.Volume, ctrl.PlayButtons,
            ctrl.TitleLabel, ctrl.ModeButtons, ctrl.StatisticLabel,
            ctrl.StatisticLabel, ctrl.InfoBar, ctrl.NextSongLabel,
            ctrl.StatusSpinner, ctrl.Menu, ctrl.TrayIcon, ctrl.SidebarCover
        )

        for controller_class in controller_class_list:
            controller_class(builder)

    def do_activate(self):
        window = g.gtk_builder.get_object('MainWindow')
        window.connect('delete-event', self.do_close_application)
        window.show_all()

        self.add_window(window)

    def do_startup(self):
        Gtk.Application.do_startup(self)
        # menubar = g.gtk_builder.get_object('menubar')
        # self.set_menubar(menubar)

    def do_close_application(self, window, event):
        window.destroy()
        shutdown_application()


if __name__ == '__main__':
    import sys
    sys.exit(Application().run(sys.argv[1:]))
