import sys
import logging

from gi.repository import Gtk

#from moosecat.gtk.widgets.playlist_tree_model import PlaylistTreeModel
from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget, SimplePopupMenu
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application, g


class QueuePlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self):
        PlaylistWidget.__init__(self, col_names=('<pixbuf>:', 'Artist', 'Album', 'Title'))
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
            self.set_model(PlaylistTreeModel(
                [(
                    'gtk-yes' if song.queue_id == queue_id else 'gtk-apply',
                    song.artist, song.album, song.title, song.queue_id
                 )
                 for song in playlist
                ],
                n_columns=4
            ))

    def do_row_activated(self, row):
        *_, queue_id = row
        print('playing', queue_id)
        g.client.player_play(queue_id=queue_id)

    ###########################
    #  Menu Signal Callbacks  #
    ###########################

    def _on_menu_clear(self, menu_item):
        print(menu_item)



class NaglfarWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.Window.__init__(self, title="Naglfar", application=app)
        self.set_default_size(300, 450)

        # a scrollbar for the child widget (that is going to be the textview)
        scrolled_window = Gtk.ScrolledWindow()
        scrolled_window.set_border_width(5)
        # we scroll only if needed
        scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        widget = QueuePlaylistWidget()

        # textview is scrolled
        scrolled_window.add(widget)

        self.add(scrolled_window)


class NaglfarApplication(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base(verbosity=logging.DEBUG)
        boot_metadata()
        boot_store()

    def do_activate(self):
        win = NaglfarWindow(self)
        win.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)

if __name__ == '__main__':
    app = NaglfarApplication()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
