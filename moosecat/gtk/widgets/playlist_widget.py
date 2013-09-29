from queue import Queue, Empty
from gi.repository import Gtk, GLib


import logging
LOGGER = logging.getLogger('playlist_widget')


class PlaylistWidget(Gtk.VBox):
    def __init__(self):
        Gtk.Box.__init__(self)
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
                Gtk.PolicyType.AUTOMATIC,
                Gtk.PolicyType.AUTOMATIC
        )

        # GtkSearchEntry configuration
        self._entry = Gtk.SearchEntry()
        self._entry.connect('changed', self._on_entry_changed)
        self._set_entry_icon('gtk-dialog-info', primary=False)

        # Treeview Configuration
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

        self._scw.add(self._view)
        self.pack_start(self._scw, True, True, 1)
        self.pack_start(self._entry, False, False, 1)
        self.pack_start(Gtk.HSeparator(), False, False, 1)

        # Typing Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        # Fill it initially with the full Queue
        self._search_queue.put('*')
        GLib.timeout_add(150, self._internal_search)

        self.show_all()

    def _set_entry_icon(self, stock_name, primary=False):
        position = Gtk.EntryIconPosition.PRIMARY if primary else Gtk.EntryIconPosition.SECONDARY
        self._entry.set_icon_from_stock(position, stock_name)

    def _on_entry_changed(self, entry):
        self._search_queue.put(entry.get_text())

    def _internal_search(self):
        try:
            query_text = None
            while True:
                query_text = self._search_queue.get_nowait()
        except Empty:
            if query_text is not None:
                try:
                    self.do_search(query_text)
                except:
                    LOGGER.exception('Error while do_search')
            return True

    def do_search(self, query):
        pass
        #self._view.set_model(PlaylistTreeModel([(str(i), str(i), str(i)) for i in range(20)]))
