from gi.repository import Gtk, GdkPixbuf, GLib

# Mooscat stuff
from moosecat.plugins import IGtkBrowser
from moosecat.core import parse_query, QueryParseException
from moosecat.boot import g


from queue import Queue, Empty


class QueueTreeBrowser(IGtkBrowser):
    def do_build(self):
        self._box = Gtk.VBox()
        scw = Gtk.ScrolledWindow()
        scw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        self._tree_store = Gtk.TreeStore(str)

        self._entry = Gtk.SearchEntry()
        self._set_entry_icon('gtk-find', True)
        self._set_entry_icon('gtk-dialog-info')

        self._view = Gtk.TreeView(model=None)
        self._view.set_fixed_height_mode(True)
        self._view.set_rules_hint(True)
        self._view.set_level_indentation(10)
        self._view.set_enable_tree_lines(True)

        for i, col in enumerate(['Songs']):
            renderer = Gtk.CellRendererText()
            renderer.set_fixed_height_from_font(1)

            col = Gtk.TreeViewColumn(col, renderer, markup=i)
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

        # Typing Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        # Fill it initially with the full Queue
        self._search_queue.put('*')
        GLib.timeout_add(200, self._search)

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

        songs_data = {}
        try:
            with g.client.store.query(query_text) as songs:
                for song in songs:
                    albums = songs_data.setdefault(song.artist, {})
                    titles = albums.setdefault(song.album, [])
                    titles.append(song.track + ' ' + song.title)

        except QueryParseException as e:
            if len(songs_data) > 0:
                self._set_entry_icon('gtk-dialog-warning')
            else:
                self._set_entry_icon('gtk-dialog-error')
            return True
        else:
            self._set_entry_icon('gtk-dialog-info')

        current_len = len(songs_data)
        if self._last_length == current_len:
            return True
        else:
            self._last_length = len(songs)

        self._tree_store.clear()

        for artist, albums in songs_data.items():
            artist_tree_iter = self._tree_store.append(None, ('<big><b>' + artist + '</b></big>', ))
            for album, titles in albums.items():
                album_tree_iter = self._tree_store.append(artist_tree_iter, ('<b>' + album + '</b>', ))
                for title in titles:
                    self._tree_store.append(album_tree_iter, (title, ))

        self._view.expand_all()
        self._view.set_model(self._tree_store)

        return True

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._box

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'QueueTree'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND_AND_REPLACE

    def priority(self):
        return 95


# if __name__ == '__main__':
#     from moosecat.boot import boot_base
#     boot_base()
#
#     win = Gtk.Window()
#     win.add(QueueTreeBrowser().get_root_widget())
#     win.show_all()
#
#     Gtk.main()
