import sys
import logging

from gi.repository import Gtk
from gi.repository import GLib
from gi.repository import GdkPixbuf

#from moosecat.gtk.widgets.playlist_tree_model import PlaylistTreeModel
from moosecat.gtk.widgets import PlaylistTreeModel, PlaylistWidget, SimplePopupMenu
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application, g
from moosecat.core import Idle


# External:
from munin.easy import EasySession
import munin
from munin.scripts.moodbar_visualizer import read_moodbar_values
from munin.scripts.moodbar_visualizer import draw_moodbar


MUNIN_SESSION = EasySession.from_name()


###########################################################################
#                              Util widgets                               #
###########################################################################

class NotebookTab(Gtk.Box):
    def __init__(self, label, symbol='<big><b>♬</b></big> '):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        # A nice music note at the left for some visual candy
        icon = Gtk.Label()
        icon.set_use_markup(True)
        icon.set_markup('<big><big>' + symbol + '</big></big> ')

        # Pack them in the container box
        self.pack_start(icon, True, True, 0)
        self.pack_start(Gtk.Label(label), True, True, 0)
        self.show_all()


class QueuePlaylistWidget(PlaylistWidget):
    'The content of a Notebook Tab, implementing a custom search for Playlists'
    def __init__(self):
        PlaylistWidget.__init__(self, col_names=(
            '<pixbuf>:',
            'ID',
            'Artist',
            'Album',
            'Title',
            '<progress>:Playcount'
        ))

        menu = SimplePopupMenu()
        menu.simple_add('Clear', self._on_menu_clear, stock_id='gtk-clear')
        self.set_menu(menu)

        g.client.signal_add('client-event', self._on_client_event)

    ######################
    #  Signal Callbacks  #
    ######################

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            self._on_entry_changed(self._entry)

    #######################
    #  Interface Methods  #
    #######################

    def do_search(self, query):
        # Get the QueueId of the currently playing song.
        queue_id = -1
        with g.client.lock_currentsong() as song:
            if song is not None:
                queue_id = song.queue_id

        with g.client.store.query(query, queue_only=True) as playlist:
            self.set_model(PlaylistTreeModel(
                list(map(lambda song: (
                    # Visible columns:
                    'gtk-yes' if song.queue_id == queue_id else '',
                    '#' + str(MUNIN_SESSION.mapping[:song.uri]),
                    song.artist,
                    song.album,
                    song.title,
                    MUNIN_SESSION.playcount(MUNIN_SESSION.mapping[:song.uri]),
                    # Hidden data:
                    song.queue_id,
                    song.uri
                ), playlist)),
                n_columns=5
            ))

    def do_row_activated(self, row):
        queue_id, uri = row[-2], row[-1]
        print('playing', queue_id, uri)
        g.client.player_play(queue_id=queue_id)
        MUNIN_SESSION.feed_history(MUNIN_SESSION.mapping[:uri])

    ###########################
    #  Menu Signal Callbacks  #
    ###########################

    def _on_menu_clear(self, menu_item):
        print(menu_item)


###########################################################################
#                               Statistics                                #
###########################################################################

class GraphPage(Gtk.ScrolledWindow):
    def __init__(self, width=1500, height=1500):
        Gtk.ScrolledWindow.__init__(self)

        path = '/tmp/.munin_plot.png'
        munin.plot.Plot(
            MUNIN_SESSION.database,
            width, height,
            path=path
        )

        # TODO: Load this directly from surface.
        #       This should be done with:
        #       But I was unable to find this function in the
        #       pygobject wrapper.. which is somewhat weird.
        pixbuf = GdkPixbuf.Pixbuf.new_from_file(path)
        image = Gtk.Image.new_from_pixbuf(pixbuf)
        image.set_size_request(width, height)
        self.add(image)
        self.show_all()


class HistoryWidget(Gtk.VBox):
    def __init__(self, history, topic):
        Gtk.VBox.__init__(self)
        self.set_size_request(100, 100)

        self._history = history

        topic_label = Gtk.Label()
        topic_label.set_use_markup(True)
        topic_label.set_markup('<b>' + topic + '</b>')
        topic_label.set_alignment(0.01, 0.5)
        self.pack_start(topic_label, False, False, 1)

        self._model = Gtk.ListStore(int, str, str)
        view = Gtk.TreeView(model=self._model)
        view.append_column(
            Gtk.TreeViewColumn('Group', Gtk.CellRendererText(), text=0)
        )
        view.append_column(
            Gtk.TreeViewColumn('UID', Gtk.CellRendererText(), text=1)
        )
        view.append_column(
            Gtk.TreeViewColumn('File', Gtk.CellRendererText(), text=2)
        )
        self.pack_start(view, True, True, 1)

        self.update()
        GLib.timeout_add(10000, self.update)

    def update(self):
        self._model.clear()
        for idx, group in enumerate(self._history.groups()):
            for munin_song in group:
                self._model.append((
                    idx + 1,
                    '#' + str(munin_song.uid),
                    MUNIN_SESSION.mapping[munin_song.uid:]
                ))


class HistoryPage(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)

        box = Gtk.HBox(self)
        box.pack_start(
            HistoryWidget(MUNIN_SESSION.listen_history, 'Listening History'),
            True, True, 2
        )
        box.pack_start(
            HistoryWidget(MUNIN_SESSION.recom_history, 'Recommendation History'),
            True, True, 2
        )
        self.add(box)


class ExamineSongPage(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)

        box = Gtk.VBox()

        self._model = Gtk.ListStore(str, str, str)
        view = Gtk.TreeView(model=self._model)

        view.append_column(
            Gtk.TreeViewColumn('Attribute', Gtk.CellRendererText(), text=0)
        )
        view.append_column(
            Gtk.TreeViewColumn('Guessed Original', Gtk.CellRendererText(), text=1)
        )
        view.append_column(
            Gtk.TreeViewColumn('Value', Gtk.CellRendererText(), text=2)
        )

        g.client.signal_add('client-event', self._on_client_event)

        self._title_label = Gtk.Label()
        self._title_label.set_use_markup(True)

        self._moodbar_da = Gtk.DrawingArea()
        self._moodbar_da.connect('draw', self._on_draw)
        self._moodbar_da.set_size_request(500, 150)
        self._moodbar_da.set_property('margin', 10)
        self._moodbar = None

        box.pack_start(self._title_label, False, False, 1)
        box.pack_start(self._moodbar_da, True, True, 1)
        box.pack_start(view, True, True, 1)

        self.add(box)
        self.show_all()

    def _on_draw(self, area, ctx):
        if self._moodbar is None:
            return True

        alloc = area.get_allocation()
        draw_moodbar(ctx, self._moodbar, alloc.width, alloc.height)

        return True

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            self.update()

    def update(self):
        self._model.clear()
        with g.client.lock_currentsong() as song:
            self._title_label.set_markup('<b>Current Song Attributes </b><i>({})</i>:'.format(song.uri))
            munin_song_id = MUNIN_SESSION.mapping[:song.uri]
            for attribute, value in MUNIN_SESSION[munin_song_id]:
                try:
                    original = getattr(song, attribute)
                except:
                    original = ''
                self._model.append((attribute, original, str(value)))

            # TODO:
            import os
            moodbar_path = os.path.join('/mnt/testdata/', song.uri) + '.mood'
            try:
                self._moodbar = read_moodbar_values(moodbar_path)
            except OSError:
                print('no moodbar for', moodbar_path)


class RulesPage(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)

        self._model = Gtk.ListStore(str, str, str, int, int)
        view = Gtk.TreeView(model=self._model)

        col = Gtk.TreeViewColumn('', Gtk.CellRendererText(), text=0)
        col.set_min_width(50)
        view.append_column(col)

        col = Gtk.TreeViewColumn('Left Side', Gtk.CellRendererText(), text=1)
        col.set_min_width(100)
        view.append_column(col)

        col = Gtk.TreeViewColumn('Right Side', Gtk.CellRendererText(), text=2)
        col.set_min_width(100)
        view.append_column(col)

        col = Gtk.TreeViewColumn('Support', Gtk.CellRendererText(), text=3)
        col.set_min_width(75)
        view.append_column(col)

        view.append_column(
            Gtk.TreeViewColumn(
                'Rating',
                Gtk.CellRendererProgress(),
                text=4,
                value=4
            )
        )

        self._view = view
        self.update()

        # TODO: For now just update every 10s.
        GLib.timeout_add(10000, self.update)

    def update(self):
        self._model.clear()
        rules = enumerate(MUNIN_SESSION.rule_index, start=1)
        for idx, (lefts, rights, support, rating) in rules:
            self._model.append((
                '#{}'.format(idx),
                str([song.uid for song in lefts]),
                str([song.uid for song in rights]),
                support,
                int(rating * 100)
            ))

        child = self.get_child()
        if child is not None:
            self.remove(child)

        if len(self._model):
            self.add(self._view)
        else:
            info_label = Gtk.Label()
            info_label.set_markup('''\
                <big><big>♫</big></big>\n\
                <b>No rules yet.</b>\n\
                <small><i>(Use the app till some appear!)</i></small>\
            ''')
            info_label.set_justify(Gtk.Justification.CENTER)
            self.add(info_label)


###########################################################################
#                           Notebook Container                            #
###########################################################################


class RecomControl(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        # Add button:
        self._add_button = Gtk.Button()
        self._add_button.set_image(
            Gtk.Image.new_from_stock('gtk-execute', Gtk.IconSize.MENU)
        )
        #self._add_button.set_relief(Gtk.ReliefStyle.NONE)
        #self._add_button.set_focus_on_click(False)
        self._add_button.connect('clicked', self._on_add_button_clicked)

        self._spin_button = Gtk.SpinButton.new_with_range(1, 1000, 1)
        self._sieve_check = Gtk.ToggleButton('Filter')
        self._attrs_entry = Gtk.Entry()
        self._attrs_entry.set_placeholder_text('<Enter Attribute Search>')

        rbox = Gtk.HBox()
        lbox = Gtk.HBox()

        lbox.pack_start(self._attrs_entry, True, False, 0)
        lbox.pack_start(self._spin_button, True, False, 0)
        rbox.pack_start(self._sieve_check, True, False, 0)
        rbox.pack_start(self._add_button, True, False, 0)

        style_context = lbox.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        style_context = rbox.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        self.pack_start(lbox, True, True, 2)
        self.pack_start(rbox, True, True, 2)

    def _on_add_button_clicked(self, button):
        pass


class NaglfarContainer(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        self._notebook = Gtk.Notebook()

        self._recom_control = RecomControl()
        self._notebook.set_action_widget(self._recom_control, Gtk.PackType.END)
        self._recom_control.show_all()

        self.pack_start(self._notebook, True, True, 1)

        self._notebook.append_page(
            QueuePlaylistWidget(), NotebookTab('Database', 'ʘ')
        )
        self._notebook.append_page(
            QueuePlaylistWidget(), NotebookTab('Playlist', '◎')
        )
        self._notebook.append_page(
            RulesPage(), NotebookTab('Rules', '💡')
        )
        self._notebook.append_page(
            ExamineSongPage(), NotebookTab('Examine', '☑')
        )
        self._notebook.append_page(
            HistoryPage(), NotebookTab('History', '⪜')
        )
        self._notebook.append_page(
            GraphPage(), NotebookTab('Graph', '☊')
        )

        self.show_all()


###########################################################################
#                           GApplication Stuff                            #
###########################################################################


class NaglfarWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.Window.__init__(self, title="Naglfar", application=app)
        self.set_default_size(300, 450)

        # a scrollbar for the child widget (that is going to be the textview)
        scrolled_window = Gtk.ScrolledWindow()

        # textview is scrolled
        # scrolled_window.add(QueuePlaylistWidget())
        container = NaglfarContainer()
        scrolled_window.add(container)

        self.add(scrolled_window)


class NaglfarApplication(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base(verbosity=logging.DEBUG)

        # TODO:
        g.client.disconnect()
        g.client.connect(port=6601)

        boot_metadata()
        boot_store()

    def do_activate(self):
        win = NaglfarWindow(self)
        win.connect('delete-event', self.do_close_application)
        win.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        MUNIN_SESSION.save()
        window.destroy()
        shutdown_application()

if __name__ == '__main__':
    app = NaglfarApplication()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
