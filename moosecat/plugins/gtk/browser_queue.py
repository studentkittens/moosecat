from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget, SimplePopupMenu
from moosecat.plugins import IGtkBrowser
from moosecat.boot import g
from gi.repository import Gtk, Gdk


class QueuePlaylistWidget(PlaylistWidget):
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

        with g.client.store.query(query, queue_only=True) as playlist:
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


from moosecat.gtk.utils import plyr_cache_to_pixbuf
from moosecat.metadata import configure_query_by_current_song, update_needed
from moosecat.core import Idle, Status
from moosecat.gtk.utils import add_keybindings


class SidebarCover(Gtk.Image):
    def __init__(self):
        Gtk.Image.__init__(self)

        self.set_size_request(250, 250)
        g.client.signal_add('client-event', self._on_client_event)
        self._last_query = None

    def _on_client_event(self, client, event):
        if not event & Idle.PLAYER:
            return

        qry = configure_query_by_current_song('cover')
        if not update_needed(self._last_query, qry):
            # LOGGER.debug('sidebar cover needs no update')
            return

        self._submit_tmstmp = g.meta_retriever.push(
            qry,
            self._on_item_retrieved
        )

    def _on_item_retrieved(self, order):
        if order.results and self._submit_tmstmp <= order.timestamp:
            cache, *_ = order.results
            pixbuf = plyr_cache_to_pixbuf(cache, width=150, height=150)
            if pixbuf is not None:
                self.set_from_pixbuf(pixbuf)
                self._last_query = order.query


class QueueBrowser(IGtkBrowser):
    def do_build(self):
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
            Gtk.PolicyType.AUTOMATIC,
            Gtk.PolicyType.AUTOMATIC
        )

        self._scw.add(QueuePlaylistWidget())
        self._overlay = Gtk.Overlay()
        self._overlay.add(self._scw)

        self._box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self._label = Gtk.Label('<span bgcolor="#AAAAAA"><b>   Metadata:   </b></span>\n(Use Ctrl-T to toggle)')
        self._label.set_use_markup(True)
        self._label.set_size_request(250, -1)
        self._label.override_background_color(0, Gdk.RGBA(0.5, 0.5, 1.0, 0.2))
        self._label.set_halign(Gtk.Align.END)
        self._label.set_alignment(0.5, 0.4)
        self._box.set_halign(Gtk.Align.END)

        self._box.pack_start(SidebarCover(), True, True, 10)

        self._overlay.add_overlay(self._label)
        self._overlay.add_overlay(self._box)
        self._overlay.show_all()

        add_keybindings(self._scw.get_child(), {
            '<Ctrl>t':  self._on_toggle_sidebar
        })

    def _on_toggle_sidebar(self, playlist, key):
        if self._label.is_visible():
            self._label.hide()
            self._box.hide()
        else:
            self._label.show()
            self._box.show_all()

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._overlay

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Queue'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND

    def priority(self):
        return 90
