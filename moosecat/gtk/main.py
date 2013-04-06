#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, GLib


from moosecat.gtk.widgets import BarSlider, ProgressSlider

from moosecat.boot import boot_base, boot_store, shutdown_application, g
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.data_tree_model import DataTreeModel

from queue import Queue, Empty

from time import time
from contextlib import contextmanager


@contextmanager
def timing(msg):
    now = time()
    yield
    print(msg, ":", time() - now)

class Demo:
    def __init__(self):
        box = Gtk.VBox()
        ent = Gtk.Entry()
        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        self.box = box

        self._view = Gtk.TreeView(model=None)
        self._view.set_fixed_height_mode(True)
        self._view.set_rules_hint(True)

        for i, col in enumerate(['Artist', 'Album', 'Title']):
            renderer = Gtk.CellRendererText()
            renderer.set_fixed_height_from_font(1)

            col = Gtk.TreeViewColumn(col, renderer, text=i)
            col.set_min_width(250)
            col.set_sizing(Gtk.TreeViewColumnSizing.FIXED)
            self._view.append_column(col)

        self._status_label = Gtk.Label('!')
        bottom_box = Gtk.HBox()
        bottom_box.pack_start(ent, True, True, 0)
        bottom_box.pack_start(self._status_label, False, False, 0)

        # Packing
        scw.add(self._view)
        box.pack_start(scw, True, True, 1)
        box.pack_start(Gtk.HSeparator(), False, False, 1)
        box.pack_start(bottom_box, False, False, 1)

        # Signals
        ent.connect('changed', self._changed)

        # Type Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        GLib.timeout_add(250, self._search)

    def _changed(self, entry):
        self._search_queue.put(entry.get_text())

    def _search(self):
        try:
            query_text = None
            while True:
                query_text = self._search_queue.get_nowait()
        except Empty:
            if query_text is None:
                return True

        with timing('select'):
            songs = []
            try:
                songs = g.client.store.query(query_text) or []
            except QueryParseException as e:
                self._status_label.set_text(' ' + e.msg + ' ')
                return True
            else:
                self._status_label.set_text('  Ready  ')

        current_len = len(songs)
        if self._last_length == current_len:
            return True
        else:
            self._last_length = len(songs)

        with timing('drawing'):
            self._view.set_model(DataTreeModel([]))
            self._view.set_model(DataTreeModel(songs))

        return True

    def _destroy(self, window):
        Gtk.main_quit()
        shutdown_application()



if __name__ == '__main__':
    builder = Gtk.Builder()
    builder.add_from_file('/home/sahib/dev/moosecat/moosecat/gtk/ui/main.glade')

    window = builder.get_object('MainWindow')
    window.connect('delete-event', Gtk.main_quit)

    builder.get_object('timeslider_align').add(ProgressSlider())
    builder.get_object('volumeslider_align').add(BarSlider())


    import logging
    boot_base(verbosity=logging.WARNING)
    boot_store(wait=False)
    g.client.store.wait_mode = True

    g.client.store.wait()
    app = Demo()

    main_pane = builder.get_object('main_pane')
    main_pane.add(app.box)

    window.show_all()
    Gtk.main()
