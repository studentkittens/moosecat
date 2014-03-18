from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget, SimplePopupMenu
from moosecat.gtk.browser import GtkBrowser
from moosecat.boot import g

from gi.repository import Gtk


class DatabasePlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self):
        PlaylistWidget.__init__(self, col_names=(
            '<pixbuf>:', 'Track', 'Artist', 'Album', 'Title', 'Date', 'Genre'
        ))
        self._create_menu()

    def _create_menu(self):
        menu = SimplePopupMenu()
        menu.simple_add('Clear', self._on_menu_clear, stock_id='gtk-clear')
        self.set_menu(menu)

    def do_search(self, query):
        # Get the QueueId of the currently playing song.
        queue_id = -1
        with g.client.lock_currentsong() as song:
            if song is not None:
                queue_id = song.queue_id

        with g.client.store.query(query, queue_only=False) as playlist:
            self.set_model(PlaylistTreeModel([
                ('gtk-yes' if song.queue_id == queue_id else 'gtk',
                song.track,
                song.artist or song.album_artist,
                song.album,
                song.title,
                song.date,
                song.genre,
                song.queue_id) for song in playlist
            ], n_columns=7))

    def do_row_activated(self, row):
        *_, queue_id = row
        print('playing', queue_id)
        g.client.player_play(queue_id=queue_id)

    ###########################
    #  Menu Signal Callbacks  #
    ###########################

    def _on_menu_clear(self, menu_item):
        print(menu_item)


class DatabaseBrowser(GtkBrowser):
    def do_build(self):
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
            Gtk.PolicyType.AUTOMATIC,
            Gtk.PolicyType.AUTOMATIC
        )

        self._scw.add(DatabasePlaylistWidget())
        self._scw.show_all()

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._scw

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Database'

    def get_browser_icon_name(self):
        return 'system-search'

    def priority(self):
        return 89
