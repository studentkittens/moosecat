from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget, SimplePopupMenu
from moosecat.gtk.browser import GtkBrowser
from moosecat.boot import g

from gi.repository import GLib, Gtk, Gdk


from contextlib import contextmanager
import time


@contextmanager
def timeit(topic):
    start = int(round(time.time() * 1000))
    yield
    end = int(round(time.time() * 1000))
    print(topic, 'took', end - start, 'ms')


class QueuePlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self):
        PlaylistWidget.__init__(self, col_names=(
            ('<pixbuf>:', 30, 0.0),
            ('Track', 50, 1.0),
            ('Artist', 150, 0.0),
            ('Album', 200, 1.0),
            ('Title', 250, 0.0),
            ('Date', 100, 0),
            ('Genre', 200, 1)
        ))
        self._create_menu()
        GLib.timeout_add(100, lambda: self.do_search('*'))

    def _create_menu(self):
        menu = SimplePopupMenu()
        menu.simple_add('Clear', self._on_menu_clear, stock_id='gtk-clear')
        self.set_menu(menu)

    def do_search(self, query):
        with timeit('set_none'):
            self.set_model(None)

        # Get the QueueId of the currently playing song.
        queue_id = -1
        with g.client.lock_currentsong() as current_song:
            if current_song is not None:
                queue_id = current_song.queue_id

        with g.client.store.query(query, queue_only=True) as playlist:
            with timeit('create_model'):
                model = PlaylistTreeModel([
                    ('gtk-yes' if song.queue_id == queue_id else 'gtk',
                        song.track,
                        song.artist or song.album_artist,
                        song.album,
                        song.title,
                        song.date,
                        song.genre,
                        song.queue_id) for song in playlist
                ], n_columns=7)
            with timeit('set_model'):
                self.set_model(model)

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


class QueueBrowser_(GtkBrowser):
    def do_build(self):
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
            Gtk.PolicyType.AUTOMATIC,
            Gtk.PolicyType.AUTOMATIC
        )

        self._scw.add(QueuePlaylistWidget())
        self._overlay = Gtk.Overlay()
        self._overlay.add(self._scw)

        self._label = Gtk.Label('')
        self._label.set_use_markup(True)
        self._label.set_size_request(250, -1)
        self._label.override_background_color(0, Gdk.RGBA(0.5, 0.5, 1.0, 0.2))
        self._label.set_halign(Gtk.Align.END)
        self._label.set_alignment(0.5, 0.4)

        self._box = Gtk.Grid()
        self._box.set_halign(Gtk.Align.END)
        cover = SidebarCover()
        cover.set_margin_top(500)
        cover.set_alignment(0.5, 1.0)
        self._box.attach_next_to(cover, None, Gtk.PositionType.BOTTOM, 1, 1)

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

def remove_ctrlf():
    # TODO: Hack, write nicer.
    css_provider = Gtk.CssProvider()
    css_provider.load_from_data("""
        @binding-set unbind_search {
            unbind "<Control>f";
        }

        GtkTreeView {
            gtk-key-bindings: unbind_search;
        }
    """.encode('utf-8'))

    context = Gtk.StyleContext()
    context.add_provider_for_screen(
        Gdk.Screen.get_default(),
        css_provider,
        Gtk.STYLE_PROVIDER_PRIORITY_USER
    )


class QueueBrowser(GtkBrowser):
    def do_build(self):
        from moosecat.gtk.completion import FishySearchOverlay

        playlist_widget = QueuePlaylistWidget()

        def search_changed(entry, query, playlist_widget):
            playlist_widget.do_search(query)

        remove_ctrlf()

        self._overlay = FishySearchOverlay()
        self._overlay.get_entry().connect(
            'search-changed', search_changed, playlist_widget
        )
        self._overlay.add(playlist_widget)
        GLib.timeout_add(500, lambda: self._overlay.show())

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
