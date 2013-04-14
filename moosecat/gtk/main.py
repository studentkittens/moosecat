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
        builder.add_from_file('/home/sahib/dev/moosecat/moosecat/gtk/ui/main.glade')

        ctrl.Sidebar(builder)
        ctrl.Timeslide(builder)
        ctrl.Volume(builder)

    def do_activate(self):
        window = g.gtk_builder.get_object('MainWindow')
        window.connect('delete-event', self.do_close_application)
        window.show_all()

        self.add_window(window)

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        Gtk.main_quit()
        shutdown_application()

if __name__ == '__main__':
    import sys
    app = Application()
    rc = app.run(sys.argv[1:])
    sys.exit(rc)
