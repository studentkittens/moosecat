#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, GdkPixbuf, GLib


from moosecat.gtk.widgets import BarSlider, ProgressSlider

from moosecat.boot import boot_base, boot_store, shutdown_application, g
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.playlist_tree_model import DataTreeModel

from queue import Queue, Empty

from time import time
from contextlib import contextmanager


@contextmanager
def timing(msg):
    now = time()
    yield
    print(msg, ":", time() - now)


class QueueBrowser:
    def __init__(self):
        box = Gtk.VBox()
        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        self._entry = Gtk.Entry()
        self._set_entry_icon('gtk-find', True)
        self._set_entry_icon('gtk-dialog_info')

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

        # Packing
        scw.add(self._view)
        box.pack_start(scw, True, True, 1)
        box.pack_start(self._entry, False, False, 1)
        box.pack_start(Gtk.HSeparator(), False, False, 1)

        # Signals
        self._entry.connect('changed', self._changed)

        # Type Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        GLib.timeout_add(250, self._search)

    def _changed(self, entry):
        self._search_queue.put(entry.get_text())

    def _set_entry_icon(self, stock_name, primary=False):
        position = Gtk.EntryIconPosition.PRIMARY if primary else Gtk.EntryIconPosition.SECONDARY
        self._entry.set_icon_from_stock(position, stock_name)

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
                if len(songs) > 0:
                    self._set_entry_icon('gtk-dialog-warning')
                else:
                    self._set_entry_icon('gtk-dialog-error')
                return True
            else:
                self._set_entry_icon('gtk-dialog-info')


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


def fill_some_icons(treeview):
    store = Gtk.ListStore(GdkPixbuf.Pixbuf, str)

    contents = [
        (Gtk.STOCK_CDROM, 'Now Playing'),
        (Gtk.STOCK_FIND, 'Queue'),
        (Gtk.STOCK_FIND_AND_REPLACE, 'Stored Playlists'),
        (Gtk.STOCK_HARDDISK, 'Database'),
        (Gtk.STOCK_YES, 'Settings'),
        (Gtk.STOCK_HELP, 'Statistics'),
        (Gtk.STOCK_JUSTIFY_FILL, 'last.fm')
    ]

    for icon, entry in contents:
        store.append((treeview.render_icon_pixbuf(icon, Gtk.IconSize.DND), entry))

    treeview.append_column(Gtk.TreeViewColumn("Icon", Gtk.CellRendererPixbuf(), pixbuf=0))
    treeview.append_column(Gtk.TreeViewColumn("Text", Gtk.CellRendererText(), text=1))
    treeview.set_headers_visible(False)

    treeview.set_model(store)


if __name__ == '__main__':
    builder = Gtk.Builder()
    builder.add_from_file('/home/sahib/dev/moosecat/moosecat/gtk/ui/main.glade')

    plugin_view = builder.get_object('plugin_view')
    fill_some_icons(plugin_view)

    window = builder.get_object('MainWindow')
    window.connect('delete-event', Gtk.main_quit)

    builder.get_object('timeslider_align').add(ProgressSlider())
    builder.get_object('volumeslider_align').add(BarSlider())

    import logging
    boot_base(verbosity=logging.INFO)
    boot_store(wait=True)
    g.client.store.wait_mode = True

    g.client.store.wait()
    app = QueueBrowser()

    main_pane = builder.get_object('main_pane')
    main_pane.add(app.box)

    window.show_all()
    Gtk.main()
