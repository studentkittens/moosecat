#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import os
import sys
import itertools
import logging
LOGGER = logging.getLogger(__name__)

# Internal:
from moosecat.gtk.widgets import PlaylistTreeModel
from moosecat.gtk.widgets import PlaylistWidget
from moosecat.gtk.widgets import SimplePopupMenu
from moosecat.gtk.widgets import ProgressSlider
from moosecat.gtk.widgets import StarSlider
from moosecat.gtk.widgets import BarSlider
from moosecat.boot import boot_base, boot_store, shutdown_application, g
from moosecat.core import Idle, Status

# External:
import munin
from munin.easy import EasySession
from munin.scripts.moodbar_visualizer import read_moodbar_values
from munin.scripts.moodbar_visualizer import draw_moodbar

# Gtk
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import Pango
from gi.repository import GdkPixbuf


class ExtraDataNamespace:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


# Globals - ugly, I know.
SESSION = EasySession.from_name()
if not hasattr(SESSION, 'data'):
    # Data that needs to be stored along the session
    SESSION.data = ExtraDataNamespace(
        attribute_search_query=None,
        recom_count=1,
        seed_song_uri=None,
        plot_needs_redraw=True,
        listen_threshold=0.5
    )


try:
    MUSIC_DIR = sys.argv[1]
except IndexError:
    MUSIC_DIR = None


###########################################################################
#                            Utility Functions                            #
###########################################################################


def parse_attribute_search(query):
    """Parse a string like this:

        'genre: metal, artist: debauchery'

    into:

        {
            'genre': 'metal',
            'artist': 'debauchery'
        }

    """
    query_dict = {}
    for pair in query.split(','):
        splits = [v.strip() for v in pair.split(':', maxsplit=1)]
        if len(splits) > 1:
            key, value = splits
            query_dict[key] = value
    return query_dict


def process_recommendation(iterator):
    """Take an iterator from the recommend_from_* family of functions
    and add the individual songs to the queue.
    """
    first = True
    with g.client.command_list_mode():
        for munin_song in iterator:
            recom_uri = SESSION.mapping[munin_song.uid]
            if first:
                SESSION.data.seed_song_uri = recom_uri
                first = False
            g.client.queue_add(recom_uri)


def format_explanation(song_uri):
    """Format the explanation of a recommendation into something like this:

        total_distance: attr1 (dist_attr1), attr2 (dist_attr2), ...

    Example:

        ~0.645: genre (0.2), lyrics(0.5), title(1.0)
    """
    if SESSION.data.seed_song_uri is None:
        return None

    munin_seed_song = SESSION.mapping[:SESSION.data.seed_song_uri]
    munin_song = SESSION.mapping[:song_uri]
    overall, detail = SESSION.explain_recommendation(
        munin_seed_song, munin_song, 10
    )

    return '{overall}: {detail}'.format(
        overall=overall,
        detail=', '.join(
            ('{} ({:1.3f})'.format(attr, dist) for attr, dist in detail)
        )
    )


def toggle_button_from_icon_name(icon_name, icon_size):
    """Helper Function that does the same as Gtk.Button.new_from_icon_name.

    But instead for Gtk.ToggleButton. Why ever there is no pendant.
    """
    toggler = Gtk.ToggleButton()
    toggler.set_image(Gtk.Image.new_from_icon_name(
        icon_name, icon_size
    ))
    return toggler


def find_icon_for_song(song, queue_id):
    """Get the icon that is shown in the Playlist/Database for a certain song.
    """
    if song.queue_id == queue_id and song.queue_id is not -1:
        return 'gtk-media-play'
    elif song.uri == SESSION.data.seed_song_uri:
        return 'gtk-media-record'
    return ''


def align_widget(widget):
    """Make a widget use minimal space"""
    alignment = Gtk.Alignment()
    alignment.set(0.5, 0.5, 0, 0)
    alignment.add(widget)
    return alignment

###########################################################################
#                              Util Widgets                               #
###########################################################################


class NotebookTab(Gtk.Box):
    """Label of a GtkNotebook Tab. Shows a unicode glyph symbol and a text.
    """
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


class InfoBar(Gtk.InfoBar):
    def __init__(self):
        Gtk.InfoBar.__init__(self)
        self.set_no_show_all(True)
        self.set_show_close_button(True)
        self.connect('response', lambda *_: self.hide())

        self._label = Gtk.Label()
        self._label.set_use_markup(True)
        self.get_content_area().add(self._label)
        self._label.show()

    def message(self, message, msg_type=Gtk.MessageType.WARNING):
        self._label.set_markup('<big>' + message + '</big>')
        self.set_message_type(msg_type)

        self.show()
        self.get_content_area().show_all()
        GLib.timeout_add(5000, self.hide)


INFO_BAR = InfoBar()

###########################################################################
#                            Playlist Widgets                             #
###########################################################################


class BasePlaylistWidget(PlaylistWidget):
    def __init__(self, queue_only):
        PlaylistWidget.__init__(self, col_names=(
            '<pixbuf>:',
            'ID',
            'Artist',
            'Album',
            'Title',
            'Date',
            '<progress>:Playcount'
        ))
        self._queue_only = queue_only
        g.client.signal_add('client-event', self._on_client_event)

    def do_search(self, query):
        # Get the QueueId of the currently playing song.
        queue_id = -1
        with g.client.lock_currentsong() as song:
            if song is not None:
                queue_id = song.queue_id

        if SESSION.data.seed_song_uri is not None:
            self._view.set_tooltip_column(7)

        with g.client.store.query(query, queue_only=self._queue_only) as songs:
            self.set_model(PlaylistTreeModel(
                list(map(lambda song: (
                    # Visible columns:
                    find_icon_for_song(song, queue_id),
                    '» ' + str(SESSION.mapping[:song.uri]),
                    song.artist,
                    song.album,
                    song.title,
                    song.date,
                    SESSION.playcount(SESSION.mapping[:song.uri]),

                    # Hidden data:
                    format_explanation(song.uri),
                    song.queue_id,
                    song.uri
                ), songs)),
                n_columns=5
            ))

    def _on_client_event(self, client, event):
        if event & (Idle.DATABASE | Idle.QUEUE | Idle.PLAYER):
            self._on_entry_changed(self._entry)


class DatabasePlaylistWidget(BasePlaylistWidget):
    def __init__(self, queue_only=False):
        BasePlaylistWidget.__init__(self, queue_only=queue_only)

        self._menu = SimplePopupMenu()
        self._menu.simple_add(
            'Recommend from this song',
            self._on_menu_recommend,
            stock_id='gtk-add'
        )
        self._menu.simple_add(
            'Clear & Recommend from this song',
            self._on_menu_recommend_clear,
            stock_id='gtk-go-down'
        )
        self._menu.simple_add(
            'Recommend from attribute search',
            self._on_menu_recommend_attrs,
            stock_id='gtk-color-picker'
        )
        self._menu.simple_add(
            'Recommend from heuristic',
            self._on_menu_recommend_heuristic,
            stock_id='gtk-about'
        )
        self.set_menu(self._menu)

    def _on_menu_recommend_clear(self, menu_item):
        g.client.queue_clear()
        self._on_menu_recommend(menu_item)

    def _on_menu_recommend(self, menu_item):
        model, rows = self.get_selected_rows()
        for row in rows:
            # row[-1] is the uri of the song.
            munin_id = SESSION.mapping[:row[-1]]
            process_recommendation(
                itertools.chain(
                    [SESSION[munin_id]],
                    SESSION.recommend_from_seed(
                        munin_id, SESSION.data.recom_count
                    )
                )
            )

    def _on_menu_recommend_attrs(self, menu_item):
        if SESSION.data.attribute_search_query is None:
            INFO_BAR.message(
                'Please fill some attributes in the top right entry!',
                Gtk.MessageType.ERROR
            )
            return

        process_recommendation(
            SESSION.recommend_from_attributes(
                SESSION.data.attribute_search_query, SESSION.data.recom_count
            )
        )

    def _on_menu_recommend_heuristic(self, menu_item):
        process_recommendation(
            SESSION.recommend_from_heuristic(SESSION.data.recom_count)
        )

    def do_row_activated(self, row):
        queue_id, uri = row[-2], row[-1]
        if queue_id > 0:
            g.client.player_play(queue_id=queue_id)
        else:
            g.client.queue_add(uri)


class QueuePlaylistWidget(DatabasePlaylistWidget):
    def __init__(self):
        DatabasePlaylistWidget.__init__(self, queue_only=True)
        self._menu.simple_add_separator()
        self._menu.simple_add(
            'Clear',
            self._on_menu_clear,
            stock_id='gtk-clear'
        )

    def _on_menu_clear(self, menu_item):
        g.client.queue_clear()

    def do_row_activated(self, row):
        queue_id, uri = row[-2], row[-1]
        g.client.player_play(queue_id=queue_id)


###########################################################################
#                            Individual Pages                             #
###########################################################################


class GraphPage(Gtk.ScrolledWindow):
    def __init__(self, width=3500, height=3500):
        Gtk.ScrolledWindow.__init__(self)

        self._area = Gtk.DrawingArea()
        self._area.set_size_request(width, height)
        self._area.connect('draw', self._on_draw)
        self._area.add_events(
            self._area.get_events()
            | Gdk.EventMask.BUTTON_PRESS_MASK
            | Gdk.EventMask.BUTTON_RELEASE_MASK
            | Gdk.EventMask.POINTER_MOTION_MASK
        )

        self._drag_mode = False
        self._last_pos = (0, 0)
        self._curr_pos = (0, 0)

        self._area.connect(
            'button-press-event', self._on_button_press_event
        )
        self._area.connect(
            'button-release-event', self._on_button_release_event
        )
        self._area.connect(
            'motion-notify-event', self._on_motion_notify
        )

        self.add(self._area)
        self.show_all()

        SESSION.data.plot_needs_redraw = True
        GLib.timeout_add(1000, self._area.queue_draw)

    def _create_vx_mapping(self):
        mapping = {}
        with g.client.store.query('*', queue_only=False) as playlist:
            for song in playlist:
                mapping[SESSION.mapping[:song.uri]] = song.title
        return mapping

    def _on_button_press_event(self, widget, event):
        self._drag_mode = True
        self._last_pos = (event.x, event.y)
        self._curr_pos = (event.x, event.y)
        return True

    def _on_button_release_event(self, widget, event):
        self._drag_mode = False
        return True

    def _on_motion_notify(self, widget, event):
        if self._drag_mode:
            self._curr_pos = (event.x, event.y)

            def _move(adjustment, diff):
                adjustment.set_value(
                    adjustment.get_value() - diff
                )

            _move(
                self.get_hadjustment(),
                self._curr_pos[0] - self._last_pos[0]
            )
            _move(
                self.get_vadjustment(),
                self._curr_pos[1] - self._last_pos[1]
            )

            self.queue_draw()
            return True

    def _on_draw(self, area, ctx):
        if SESSION.data.plot_needs_redraw or self._surface is None:
            alloc = area.get_allocation()
            self._surface = munin.plot.Plot(
                SESSION.database,
                alloc.width, alloc.height,
                do_save=True,
                vx_mapping=self._create_vx_mapping()
            )
            SESSION.data.plot_needs_redraw = False

        ctx.set_source_surface(self._surface, 0, 0)
        ctx.paint()
        return True


class HistoryWidget(Gtk.VBox):
    def __init__(self, history, topic):
        Gtk.VBox.__init__(self)
        self.set_size_request(100, 100)

        self._history = history

        topic_label = Gtk.Label()
        topic_label.set_use_markup(True)
        topic_label.set_markup('<b>' + topic + '</b>')
        topic_label.set_alignment(0.01, 0.5)

        self._model = Gtk.ListStore(int, str, str)
        self._view = Gtk.TreeView(model=self._model)
        self._view.append_column(
            Gtk.TreeViewColumn('Group', Gtk.CellRendererText(), text=0)
        )
        self._view.append_column(
            Gtk.TreeViewColumn('UID', Gtk.CellRendererText(), text=1)
        )
        self._view.append_column(
            Gtk.TreeViewColumn('File', Gtk.CellRendererText(), text=2)
        )

        self.pack_start(topic_label, False, False, 1)
        self.pack_start(self._view, True, True, 1)
        self.update()

    def update(self):
        self._model.clear()
        for idx, group in enumerate(self._history.groups()):
            for munin_song in group:
                self._model.append((
                    idx + 1,
                    '#' + str(munin_song.uid),
                    SESSION.mapping[munin_song.uid:]
                ))


class HistoryPage(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        self._listen_history = HistoryWidget(
            SESSION.listen_history, 'Listening History'
        )
        self._recom_history = HistoryWidget(
            SESSION.recom_history, 'Recommendation History'
        )

        self.pack_start(self._listen_history, True, True, 2)
        self.pack_start(self._recom_history, True, True, 2)

    def update(self):
        self._listen_history.update()
        self._recom_history.update()


class ExamineSongPage(Gtk.VBox):
    def __init__(self):
        Gtk.VBox.__init__(self)

        self._model = Gtk.ListStore(str, str, str)
        view = Gtk.TreeView(model=self._model)

        view.append_column(
            Gtk.TreeViewColumn('Attribute', Gtk.CellRendererText(), text=0)
        )
        view.append_column(
            Gtk.TreeViewColumn('Original', Gtk.CellRendererText(), text=1)
        )

        renderer = Gtk.CellRendererText()
        renderer.set_property('wrap-width', 100)
        renderer.set_property('wrap-mode', Pango.WrapMode.WORD)

        # renderer.set_property('max-width-chars', 200)
        view.append_column(
            Gtk.TreeViewColumn('Value', renderer, text=2)
        )

        self._title_label = Gtk.Label()
        self._title_label.set_use_markup(True)

        self._moodbar_da = Gtk.DrawingArea()
        self._moodbar_da.set_size_request(1000, 100)
        self._moodbar_da.connect('draw', self._on_draw)
        self._moodbar_da.set_property('margin', 15)
        self._moodbar = None

        self.pack_start(self._title_label, False, False, 1)
        self.pack_start(self._moodbar_da, False, False, 1)
        self.pack_start(view, False, False, 1)
        self.update()

        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            self.update()

    def _on_draw(self, area, ctx):
        if self._moodbar is None:
            return True

        alloc = area.get_allocation()
        draw_moodbar(ctx, self._moodbar, alloc.width, alloc.height)
        return True

    def update(self):
        self._model.clear()
        with g.client.lock_currentsong() as song:
            if song is None:
                return

        song_uri = song.uri
        self._title_label.set_markup(
            '<b>Current Song Attributes </b><i>({})</i>:'.format(
                GLib.markup_escape_text(song.uri)
            )
        )
        munin_song_id = SESSION.mapping[:song_uri]
        for attribute, value in SESSION[munin_song_id]:
            try:
                original = getattr(song, attribute)
            except AttributeError:
                original = ''
            self._model.append((attribute, original, str(value)))

        if MUSIC_DIR is not None:
            moodbar_path = os.path.join(MUSIC_DIR, song_uri) + '.mood'
            try:
                self._moodbar = read_moodbar_values(moodbar_path)
                self._moodbar_da.queue_draw()
            except OSError:
                LOGGER.warning(
                    'could not read moodbar for file: ' + moodbar_path
                )

        self.show_all()


class RulesPage(Gtk.Bin):
    def __init__(self):
        Gtk.Bin.__init__(self)

        self._model = Gtk.ListStore(str, str, str, int, int)
        self._view = Gtk.TreeView(model=self._model)

        col = Gtk.TreeViewColumn('', Gtk.CellRendererText(), text=0)
        col.set_min_width(50)
        self._view.append_column(col)

        col = Gtk.TreeViewColumn('Left Side', Gtk.CellRendererText(), text=1)
        col.set_min_width(100)
        self._view.append_column(col)

        col = Gtk.TreeViewColumn('Right Side', Gtk.CellRendererText(), text=2)
        col.set_min_width(100)
        self._view.append_column(col)

        col = Gtk.TreeViewColumn('Support', Gtk.CellRendererText(), text=3)
        col.set_min_width(75)
        self._view.append_column(col)

        self._view.append_column(
            Gtk.TreeViewColumn(
                'Rating',
                Gtk.CellRendererProgress(),
                text=4,
                value=4
            )
        )

        self.update()

    def update(self):
        self._model.clear()
        rules = enumerate(SESSION.rule_index, start=1)
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

        self.show_all()


###########################################################################
#                              Top controls                               #
###########################################################################


class RecomControl(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        # Add button:
        self._add_button = Gtk.Button()
        self._add_button.set_image(
            Gtk.Image.new_from_stock('gtk-execute', Gtk.IconSize.MENU)
        )
        self._add_button.connect('clicked', self._on_add_button_clicked)
        self._spin_button = Gtk.SpinButton.new_with_range(1, 1000, 1)
        self._spin_button.connect(
            'value-changed',
            self._on_spin_button_changed
        )
        self._spin_button.set_value(SESSION.data.recom_count or 1)

        self._sieve_check = Gtk.ToggleButton('Filter')
        self._sieve_check.set_active(SESSION.sieving)
        self._sieve_check.connect(
            'toggled',
            lambda btn: setattr(SESSION, 'sieving', btn.get_active())
        )

        self._attrs_entry = Gtk.Entry()
        self._attrs_entry.set_placeholder_text('<Enter Attribute Search>')
        self._attrs_entry.connect('activate', self._on_add_button_clicked)
        self._attrs_entry.connect('changed', self._on_entry_changed)

        self._percent_button = Gtk.ScaleButton()
        self._percent_button.set_adjustment(
            Gtk.Adjustment(SESSION.data.listen_threshold, 0.0, 1.0, 0.01)
        )
        self._percent_button.set_icons(['gtk-cdrom'])
        self._percent_button.set_relief(Gtk.ReliefStyle.NORMAL)
        self._percent_button.connect(
            'value-changed',
            self._on_listen_threshold_changed
        )

        rbox = Gtk.HBox()
        lbox = Gtk.HBox()

        lbox.pack_start(self._attrs_entry, True, False, 0)
        lbox.pack_start(self._spin_button, True, False, 0)
        rbox.pack_start(self._sieve_check, True, False, 0)
        rbox.pack_start(self._percent_button, True, False, 0)
        rbox.pack_start(self._add_button, True, False, 0)

        style_context = lbox.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        style_context = rbox.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        alignment = Gtk.Alignment()
        alignment.set(0.5, 0.5, 0, 0)
        alignment.add(rbox)

        self._star_slider = StarSlider()
        self._star_slider.set_size_request(
            self._star_slider.width_multiplier() * 20,
            20
        )
        self._star_slider.connect('percent-change', self._on_stars_changed)
        g.client.signal_add('client-event', self._on_client_event)

        slide_alignment = Gtk.Alignment()
        slide_alignment.set(0.5, 0.5, 0, 0)
        slide_alignment.add(self._star_slider)

        self.pack_start(slide_alignment, True, True, 5)
        self.pack_start(lbox, True, True, 2)
        self.pack_start(alignment, True, True, 2)
        self.show_all()

    def _on_entry_changed(self, entry):
        query_dict = parse_attribute_search(entry.get_text())
        if query_dict:
            SESSION.data.attribute_search_query = query_dict
        else:
            SESSION.data.attribute_search_query = None

    def _on_add_button_clicked(self, button):
        if SESSION.data.attribute_search_query is not None:
            iterator = SESSION.recommend_from_attributes(
                SESSION.data.attribute_search_query,
                SESSION.data.recom_count
            )
        else:
            iterator = SESSION.recommend_from_heuristic(
                SESSION.data.recom_count
            )

        process_recommendation(iterator)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with g.client.lock_currentsong() as song:
                if song is None:
                    return

                current_song_uri = song.uri

            munin_song = SESSION.mapping[:current_song_uri]
            rating = SESSION[munin_song]['rating']
            if rating is None:
                self._star_slider.stars = 0
            else:
                self._star_slider.stars = rating[0]

    def _on_spin_button_changed(self, spin_button):
        SESSION.data.recom_count = self._spin_button.get_value_as_int()

    def _on_listen_threshold_changed(self, button, value):
        SESSION.data.listen_threshold = value

    def _on_stars_changed(self, slider):
        with g.client.lock_currentsong() as song:
            if song is None:
                INFO_BAR.message(
                    'There is no current song to rate.',
                    Gtk.MessageType.INFO
                )
                return

            current_song_uri = song.uri

        munin_song = SESSION[SESSION.mapping[:current_song_uri]]
        with SESSION.fix_graph():
            SESSION[SESSION.modify(
                munin_song, {'rating': slider.stars}
            )]

        SESSION.data.plot_needs_redraw = True


class ModebuttonBox(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        style_context = self.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        self._single_button = toggle_button_from_icon_name(
            'object-rotate-left', Gtk.IconSize.MENU
        )
        self._repeat_button = toggle_button_from_icon_name(
            'media-playlist-repeat', Gtk.IconSize.MENU
        )
        self._random_button = toggle_button_from_icon_name(
            'media-playlist-shuffle', Gtk.IconSize.MENU
        )
        self._consume_button = toggle_button_from_icon_name(
            'media-tape', Gtk.IconSize.MENU
        )

        self._actions = {
            self._single_button: Status.single,
            self._repeat_button: Status.repeat,
            self._random_button: Status.random,
            self._consume_button: Status.consume
        }

        self.pack_start(self._single_button, False, False, 0)
        self.pack_start(self._repeat_button, False, False, 0)
        self.pack_start(self._random_button, False, False, 0)
        self.pack_start(self._consume_button, False, False, 0)

        for button in self._actions.keys():
            button.connect('clicked', self._on_button_clicked)
        g.client.signal_add('client-event', self._on_client_event)

    def _on_button_clicked(self, button):
        # Get the corresponding action for this button
        action_func = self._actions[button]

        # Trigger the action
        with g.client.lock_status() as status:
            is_active = button.get_active()
            current_state = action_func.__get__(status)

            if is_active != current_state:
                action_func.__set__(status, is_active)

    def _on_client_event(self, client, event):
        if event & Idle.OPTIONS:
            # Update appearance according to MPD's state
            with g.client.lock_status() as status:
                self._single_button.set_active(status.single)
                self._repeat_button.set_active(status.repeat)
                self._consume_button.set_active(status.consume)
                self._random_button.set_active(status.random)


class PlaybuttonBox(Gtk.HBox):
    def __init__(self):
        Gtk.HBox.__init__(self)

        style_context = self.get_style_context()
        style_context.add_class(Gtk.STYLE_CLASS_LINKED)

        self._pause_button = Gtk.Button.new_from_icon_name(
            'media-playback-start', Gtk.IconSize.MENU
        )
        self._next_button = Gtk.Button.new_from_icon_name(
            'media-skip-forward', Gtk.IconSize.MENU
        )
        self._prev_button = Gtk.Button.new_from_icon_name(
            'media-skip-backward', Gtk.IconSize.MENU
        )
        self._stop_button = Gtk.Button.new_from_icon_name(
            'media-playback-stop', Gtk.IconSize.MENU
        )

        self._actions = {
            self._stop_button: g.client.player_stop,
            self._pause_button: g.client.player_pause,
            self._prev_button: g.client.player_previous,
            self._next_button: g.client.player_next
        }

        self.pack_start(self._prev_button, False, False, 0)
        self.pack_start(self._pause_button, False, False, 0)
        self.pack_start(self._stop_button, False, False, 0)
        self.pack_start(self._next_button, False, False, 0)

        for button in self._actions.keys():
            button.connect('clicked', self._on_button_clicked)

        g.client.signal_add('client-event', self._on_client_event)

    def _set_button_icon(self, icon):
        self._pause_button.set_image(Gtk.Image.new_from_icon_name(
            icon, Gtk.IconSize.MENU
        ))

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            with client.lock_status() as status:
                if status is None:
                    return
                if status.state == Status.Playing:
                    self._actions[self._pause_button] = client.player_pause
                    self._set_button_icon('media-playback-pause')
                elif status.state == Status.Paused:
                    self._actions[self._pause_button] = client.player_play
                    self._set_button_icon('media-playback-start')

    def _on_button_clicked(self, button):
        self._actions[button]()


###########################################################################
#                           Root of all widgets                           #
###########################################################################


class NoteBookContent(Gtk.Bin):
    def __init__(self):
        Gtk.Bin.__init__(self)

        self._notebook = Gtk.Notebook()
        self._notebook.popup_enable()
        self._last_song = None

        self._recom_control = RecomControl()
        self._notebook.set_action_widget(self._recom_control, Gtk.PackType.END)
        self.add(self._notebook)

        def append_page(widget, title, symbol):
            scrolled_window = Gtk.ScrolledWindow()
            scrolled_window.add(widget)

            self._notebook.append_page(
                scrolled_window, NotebookTab(title, symbol)
            )
            self._notebook.set_tab_reorderable(scrolled_window, True)
            return widget

        append_page(DatabasePlaylistWidget(), 'Database', 'ʘ')
        append_page(QueuePlaylistWidget(), 'Playlist', '◎')
        append_page(GraphPage(), 'Graph', '☊')

        self._rules_page = append_page(RulesPage(), 'Rules', '💡')
        self._examine_page = append_page(ExamineSongPage(), 'Examine', '☑')
        self._history_page = append_page(HistoryPage(), 'History', '⪜')

        g.client.signal_add('client-event', self._on_client_event)
        self.show_all()

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            current_song_uri = None
            with g.client.lock_currentsong() as song:
                if song is not None:
                    current_song_uri = song.uri

            listened_percent = g.heartbeat.last_listened_percent
            enough_listened = listened_percent >= SESSION.data.listen_threshold
            if self._last_song != current_song_uri and enough_listened:
                if self._last_song is not None:
                    SESSION.feed_history(
                        SESSION.mapping[:self._last_song]
                    )

            self._rules_page.update()
            self._history_page.update()
            self._notebook.show_all()
            self._last_song = current_song_uri


class MainWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.ApplicationWindow.__init__(
            self,
            title="Naglfar (libmunin demo application)",
            application=app
        )
        self.set_default_size(1400, 900)

        # application icon:
        script_path = os.path.dirname(os.path.abspath(__file__))
        pixbuf_path = os.path.join(script_path, 'logo.png')
        pixbuf = GdkPixbuf.Pixbuf.new_from_file(pixbuf_path)
        self.set_icon(pixbuf)

        self._volume_slider = BarSlider()
        self._volume_slider.set_size_request(60, 25)
        self._volume_slider.connect(
            'percent-change', self._on_volume_click_event
        )

        self._progress_bar = ProgressSlider()
        self._progress_bar.set_size_request(200, 15)
        self._progress_bar.connect(
            'percent-change',
            self._on_percent_change
        )

        self._headerbar = Gtk.HeaderBar()
        self._headerbar.set_show_close_button(True)

        self._headerbar.pack_start(PlaybuttonBox())
        self._headerbar.pack_start(align_widget(self._progress_bar))
        self._headerbar.pack_end(align_widget(self._volume_slider))
        self._headerbar.pack_end(ModebuttonBox())

        app_box = Gtk.VBox()
        app_box.pack_start(INFO_BAR, False, False, 0)
        app_box.pack_start(NoteBookContent(), True, True, 0)
        app_box.pack_start(self._headerbar, False, False, 0)
        self.add(app_box)

        GLib.timeout_add(500, self._timeout_callback)
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.MIXER:
            with client.lock_status() as status:
                if status is not None:
                    self._volume_slider.percent = status.volume / 100

        if event & Idle.PLAYER:
            with client.lock_currentsong() as song:
                if song is not None:
                    self.set_title('Naglfar (Playing: {} - {})'.format(
                        GLib.markup_escape_text(
                            song.artist or song.album_artist
                        ),
                        GLib.markup_escape_text(song.title)
                    ))
                    self._headerbar.set_title('{title}'.format(
                        title=song.title
                    ))
                    self._headerbar.set_subtitle('{album} - {artist}'.format(
                        album=song.album,
                        artist=song.artist or song.album_artist
                    ))
                else:
                    self._headerbar.set_title('Ready to play!')
                    self._headerbar.set_subtitle(
                        'Just add something to the playlist.'
                    )

    def _on_volume_click_event(self, bar):
        with g.client.lock_status() as status:
            if status is not None:
                status.volume = bar.percent * 100

    # User clicked in the Slider
    def _on_percent_change(self, slider):
        g.client.player_seek_relative(slider.percent)

    def _timeout_callback(self):
        self._progress_bar.percent = g.heartbeat.percent
        return True

###########################################################################
#                           GApplication Stuff                            #
###########################################################################


class NaglfarApplication(Gtk.Application):
    def __init__(self):
        Gtk.Application.__init__(self)

        # Bring up the core!
        boot_base(
            verbosity=logging.DEBUG,
            protocol_machine='idle',
            host='localhost',
            port=6601
        )
        boot_store()

    def do_activate(self):
        win = MainWindow(self)
        win.connect('delete-event', self.do_close_application)
        win.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        window.destroy()
        SESSION.save()
        shutdown_application()

###########################################################################
#                                  Main                                   #
###########################################################################


if __name__ == '__main__':
    if '--help' in sys.argv:
        print('{} [music_dir_root]'.format(sys.argv[0]))
        sys.exit(0)

    print(SESSION.data.seed_song_uri)

    # sometimes pickle.dump hits max recursion depth...
    sys.setrecursionlimit(10000)

    app = NaglfarApplication()
    exit_status = app.run(sys.argv[1:])
    sys.exit(exit_status)
