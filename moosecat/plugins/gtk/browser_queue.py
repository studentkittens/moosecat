from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget
from moosecat.plugins import IGtkBrowser
from moosecat.core import parse_query, QueryParseException  # TODO
from moosecat.boot import g
from gi.repository import Gtk


class QueuePlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self):
        PlaylistWidget.__init__(self)

    def do_search(self, query):
        with g.client.store.query(query, queue_only=True) as playlist:
            self._view.set_model(PlaylistTreeModel(
                [(song.artist, song.album, song.title) for song in playlist]
            ))


class QueueBrowser(IGtkBrowser):
    def do_build(self):
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
                Gtk.PolicyType.AUTOMATIC,
                Gtk.PolicyType.AUTOMATIC
        )

        self._scw.add(QueuePlaylistWidget())
        self._scw.show_all()

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._scw

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Queue'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND

    def priority(self):
        return 90
