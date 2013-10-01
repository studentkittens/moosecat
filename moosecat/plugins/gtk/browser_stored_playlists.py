from moosecat.gtk.widgets import NotebookTab, PlaylistWidget, PlaylistTreeModel
from moosecat.plugins import IGtkBrowser
from moosecat.core import Idle
from moosecat.boot import g

from gi.repository import Gtk, GObject


def _tab_label_from_page_num(notebook, page_num):
    chld = notebook.get_nth_page(page_num)
    return notebook.get_tab_label(chld)


class StoredPlaylistAddEntry(Gtk.Box):
    __gsignals__ = {
            'playlist-added': (
                    GObject.SIGNAL_RUN_FIRST,  # Run-Order
                    None,                      # Return Type
                    (str, )                    # Parameters
            )
    }

    def __init__(self, notebook_widget):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)
        self._notebook = notebook_widget

        self._add_button = Gtk.Button()
        self._add_button.set_image(
                Gtk.Image.new_from_stock('gtk-add', Gtk.IconSize.MENU)
        )
        self._add_button.set_relief(Gtk.ReliefStyle.NONE)
        self._add_button.set_focus_on_click(False)
        self._add_button.connect('clicked', self._on_add_button_clicked)
        self._add_button.set_sensitive(False)

        self._entry = Gtk.Entry()
        self._entry.set_placeholder_text('<Enter Playlist Name>')
        self._entry.connect('changed', self._on_entry_changed)

        self.pack_start(self._entry, True, True, 0)
        self.pack_start(self._add_button, True, False, 0)
        self.show_all()

    def _on_entry_changed(self, entry):
        names = []
        for page_num in range(self._notebook.get_n_pages()):
            nbtab = _tab_label_from_page_num(self._notebook, page_num)
            if hasattr(nbtab, 'get_playlist_name()'):
                names.append(nbtab.get_playlist_name())

        entry_text = entry.get_text()
        is_sensitive = bool(entry_text) and entry_text not in names
        self._add_button.set_sensitive(is_sensitive)

    def _on_add_button_clicked(self, button):
        text = self._entry.get_text()
        if text:
            self.emit('playlist-added', text)
            self._add_button.set_sensitive(False)


class StoredPlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self, playlist_name):
        PlaylistWidget.__init__(self)
        self._playlist_name = playlist_name

    def do_search(self, query):
        with g.client.store.stored_playlist_query(self._playlist_name, query) as playlist:
            self.set_model(PlaylistTreeModel(
                [(song.artist, song.album, song.title, song) for song in playlist]
            ))


class StoredPlaylistBrowser(IGtkBrowser):
    'Actual Browser Implementation putting all together'
    def do_build(self):
        self._notebook = Gtk.Notebook()
        add_entry = StoredPlaylistAddEntry(self._notebook)
        add_entry.connect('playlist-added', self._on_add_button_clicked)
        self._notebook.set_action_widget(add_entry, Gtk.PackType.END)
        self._notebook.connect('switch-page', self._on_switch_page)

        # a mapping from the playlist names to their corresponding widgets
        self._playlist_to_widget = set()

        # Start configuration
        self._empty_page_is_shown = True
        self._on_del_button_clicked(None, 0)

        g.client.signal_add('client-event', self._on_client_event)

    def _update_tab_numbers(self):
        for page_num in range(self._notebook.get_n_pages()):
            _tab_label_from_page_num(self._notebook, page_num).set_page_num(page_num)

    def _show_empty_playlists(self):
        self._empty_page_is_shown = True
        info_label = Gtk.Label()
        info_label.set_markup('''\
                <big><big>♫</big></big>\n\
                <b>No stored playlists!</b>\n\
                <small><i>(Use the <big>+</big> in the upper corner to add some!)</i></small>\
        ''')
        info_label.set_justify(Gtk.Justification.CENTER)
        info_label.show()

        # We need at lease one tab to show something.
        self._notebook.append_page(info_label, Gtk.Label(' ☉ '))
        self._notebook.show_all()

    def _update_stored_playlists(self):
        self._playlist_to_widget = set()
        for page_num in range(self._notebook.get_n_pages()):
            self._del_tab(page_num)

        for name in reversed(g.client.store.stored_playlist_names):
            self._add_tab(name)

    def _add_tab(self, playlist_name):
        if self._empty_page_is_shown is True:
            self._notebook.remove_page(0)
            self._empty_page_is_shown = False

        ntab = NotebookTab(playlist_name, page_num=self._notebook.get_n_pages())
        ntab.connect('tab-close', self._on_del_button_clicked)

        self._notebook.append_page(Gtk.VBox(), ntab)
        self._notebook.show_all()

    def _del_tab(self, page_num):
        self._notebook.remove_page(page_num)
        self._update_tab_numbers()
        if self._notebook.get_n_pages() is 0:
            self._show_empty_playlists()

    #####################
    #  Signal Handlers  #
    #####################

    def _on_client_event(self, client, event):
        if event & Idle.STORED_PLAYLIST:
            self._update_stored_playlists()

    def _on_add_button_clicked(self, _, playlist_name):
        g.client.queue_save(playlist_name)
        self._add_tab(playlist_name)

    def _on_del_button_clicked(self, _, page_num):
        self._del_tab(page_num)

    def _on_switch_page(self, notebook, page, page_num):
        nbtab = _tab_label_from_page_num(self._notebook, page_num)

        # The empty screen label has no get_playlist_name().
        if not hasattr(nbtab, 'get_playlist_name'):
            return

        # Only create new playlists widget if not already there.
        if nbtab.get_playlist_name() in self._playlist_to_widget:
            return

        # Remember the created playlist
        self._playlist_to_widget.add(nbtab.get_playlist_name())

        # Load the playlist (asynchronously)
        g.client.store.stored_playlist_load(nbtab.get_playlist_name())

        # Create a new playlist widget on demand
        playlist_widget = StoredPlaylistWidget(nbtab.get_playlist_name())

        # Fill in some life.
        box = self._notebook.get_nth_page(page_num)
        box.pack_start(Gtk.Label('Old'), False, False, 0)
        box.pack_start(playlist_widget, True, True, 0)
        box.show_all()

    #######################
    #  IGtkBRowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._notebook

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Playlists'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND_AND_REPLACE

    def priority(self):
        return 80

###########################################################################
#                      Standalone Main for debugging                      #
###########################################################################

if __name__ == '__main__':
    import sys
    from moosecat.core import Client

    c = Client()
    c.connect(port=6600)

    # Fake the g.client.store object.
    g = type('G', (), {})
    g.client = c
    g.client.store_initialize('/tmp')

    class MyApplication(Gtk.Application):
        def __init__(self):
            Gtk.Application.__init__(self)

        def do_activate(self):
            spb = StoredPlaylistBrowser()
            spb.do_build()
            win = Gtk.ApplicationWindow(application=self)
            win.add(spb.get_root_widget())
            win.show_all()

        def do_startup(self):
            Gtk.Application.do_startup(self)

    app = MyApplication()
    exit_status = app.run(sys.argv)
    c.disconnect()
    sys.exit(exit_status)
