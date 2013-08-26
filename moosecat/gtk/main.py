#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk
from moosecat.boot import boot_base, boot_store, shutdown_application, g
import moosecat.gtk.controller as ctrl


class Application(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base()
        boot_store()

        # Bind a global reference to gtk_builder
        builder = Gtk.Builder()
        g.register('gtk_builder', builder)

        # Bind a global reference to the Application Instance
        g.register('gtk_app', self)

        # TODO: Fixed path, srsly..
        builder.add_from_file('/home/sahib/dev/moosecat/moosecat/gtk/ui/main.glade')
        controller_list = [
            ctrl.Sidebar, ctrl.Timeslide, ctrl.Volume, ctrl.PlayButtons,
            ctrl.TitleLabel, ctrl.ModeButtons, ctrl.StatisticLabel,
            ctrl.StatisticLabel, ctrl.InfoBar, ctrl.NextSongLabel,
            ctrl.StatusSpinner, ctrl.Menu, ctrl.TrayIcon
        ]

        for controller in controller_list:
            controller(builder)

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
        shutdown_application()


if __name__ == '__main__':
    import sys
    sys.exit(Application().run(sys.argv[1:]))
