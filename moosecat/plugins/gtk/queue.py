from gi.repository import Gtk, GdkPixbuf, GLib

# Mooscat stuff
from moosecat.plugins import IGtkBrowser
from moosecat.core import parse_query, QueryParseException
from moosecat.boot import g


from queue import Queue, Empty

from moosecat.gtk.playlist_tree_model import PlaylistTreeModel


class QueueBrowser(IGtkBrowser):
    def do_build(self):
        self._box = Gtk.VBox()
        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        self._entry = Gtk.Entry()
        self._set_entry_icon('gtk-find', True)
        self._set_entry_icon('gtk-dialog-info')

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
        self._box.pack_start(scw, True, True, 1)
        self._box.pack_start(self._entry, False, False, 1)
        self._box.pack_start(Gtk.HSeparator(), False, False, 1)

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

        self._view.set_model(PlaylistTreeModel([]))
        self._view.set_model(PlaylistTreeModel(songs))

        return True

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._box

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Queue'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND