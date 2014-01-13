#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import os
import sys
import logging
import itertools

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
from gi.repository import GLib
from gi.repository import GdkPixbuf


class ExtraData:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


# Globals - ugly, I know
SESSION = EasySession.from_name()
if not hasattr(SESSION, 'data'):
    # Data that needs to be stored along the session
    SESSION.data = ExtraData(
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


def parse_attribute_search(query):
    query_dict = {}
    for pair in query.split(','):
        splits = [v.strip() for v in pair.split(':', maxsplit=1)]
        if len(splits) > 1:
            key, value = splits
            query_dict[key] = value
    return query_dict


def process_recommendation(iterator):
    first = False
    with g.client.command_list_mode():
        for munin_song in iterator:
            recom_uri = SESSION.mapping[munin_song.uid]
            if not first:
                SESSION.data.seed_song_uri = recom_uri
                first = True
            g.client.queue_add(recom_uri)


def format_explanation(song_uri):
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
    toggler = Gtk.ToggleButton()
    toggler.set_image(Gtk.Image.new_from_icon_name(
        icon_name, icon_size
    ))
    return toggler


def find_icon_for_song(song, queue_id):
    if song.queue_id == queue_id and song.queue_id is not -1:
        return 'gtk-media-play'
    elif song.uri == SESSION.data.seed_song_uri:
        'gtk-media-record'
    return ''


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


class BasePlaylistWidget(PlaylistWidget):
    def __init__(self, queue_only):
        PlaylistWidget.__init__(self, col_names=(
            '<pixbuf>:',
            'ID',
            'Artist',
            'Album',
            'Title',
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
            self._view.set_tooltip_column(6)

        with g.client.store.query(query, queue_only=self._queue_only) as songs:
            self.set_model(PlaylistTreeModel(
                list(map(lambda song: (
                    # Visible columns:
                    find_icon_for_song(song, queue_id),
                    '» ' + str(SESSION.mapping[:song.uri]),
                    song.artist,
                    song.album,
                    song.title,
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

    def _on_menu_recommend(self, menu_item):
        model, rows = self.get_selected_rows()
        for row in rows:
            uri = row[-1]
            munin_id = SESSION.mapping[:uri]
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


class GraphPage(Gtk.ScrolledWindow):
    def __init__(self, width=1500, height=1500):
        Gtk.ScrolledWindow.__init__(self)
        self._surface = None
        self._area = Gtk.DrawingArea()
        self._area.set_size_request(width, height)
        self._area.connect('draw', self._on_draw)
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

    def _on_draw(self, area, ctx):
        if SESSION.data.plot_needs_redraw or self._surface is None:
            alloc = area.get_allocation()
            self._surface = munin.plot.Plot(
                SESSION.database,
                alloc.width, alloc.height,
                do_save=False,
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
        self.pack_start(topic_label, False, False, 1)

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


class HistoryPage(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)

        box = Gtk.HBox(self)
        self._listen_history = HistoryWidget(
            SESSION.listen_history, 'Listening History'
        )
        self._recom_history = HistoryWidget(
            SESSION.recom_history, 'Recommendation History'
        )

        box.pack_start(self._listen_history, True, True, 2)
        box.pack_start(self._recom_history, True, True, 2)
        self.add(box)

    def update(self):
        self._listen_history.update()
        self._recom_history.update()


class ExamineSongPage(Gtk.ScrolledWindow):
    def __init__(self):
        Gtk.ScrolledWindow.__init__(self)

        self._box = Gtk.VBox()

        self._model = Gtk.ListStore(str, str, str)
        self._view = Gtk.TreeView(model=self._model)

        self._view.append_column(
            Gtk.TreeViewColumn('Attribute', Gtk.CellRendererText(), text=0)
        )
        self._view.append_column(
            Gtk.TreeViewColumn('Original', Gtk.CellRendererText(), text=1)
        )

        renderer = Gtk.CellRendererText()
        renderer.set_property('max-width-chars', 200)
        self._view.append_column(
            Gtk.TreeViewColumn('Value', renderer, text=2)
        )

        self._title_label = Gtk.Label()
        self._title_label.set_use_markup(True)

        self._moodbar_da = Gtk.DrawingArea()
        self._moodbar_da.set_size_request(1000, 100)
        self._moodbar_da.connect('draw', self._on_draw)
        self._moodbar_da.set_property('margin', 15)
        self._moodbar = None

        self._box.pack_start(self._title_label, False, False, 1)
        self._box.pack_start(self._moodbar_da, False, False, 1)
        self._box.pack_start(self._view, False, False, 1)

        self.add(self._box)
        g.client.signal_add('client-event', self._on_client_event)
        self.update()

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
            '<b>Current Song Attributes </b><i>({})</i>:'.format(song_uri)
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
                print('could not readmoodbar')

        self.show_all()


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
            rating = SESSION[munin_song]['rating'][0]
            self._star_slider.stars = rating or 0

    def _on_spin_button_changed(self, spin_button):
        SESSION.data.recom_count = self._spin_button.get_value_as_int()

    def _on_listen_threshold_changed(self, button, value):
        SESSION.data.listen_threshold = value

    def _on_stars_changed(self, slider):
        with g.client.lock_currentsong() as song:
            if song is None:
                return

            current_song_uri = song.uri

        munin_song = SESSION[SESSION.mapping[:current_song_uri]]
        with SESSION.fix_graph():
            SESSION[SESSION.modify(
                munin_song, {'rating': slider.stars}
            )]

        SESSION.data.plot_needs_redraw = True


class NaglfarContainer(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        self._notebook = Gtk.Notebook()

        self._recom_control = RecomControl()
        self._notebook.set_action_widget(self._recom_control, Gtk.PackType.END)
        self._recom_control.show_all()

        self.pack_start(self._notebook, True, True, 1)

        self._notebook.append_page(
            DatabasePlaylistWidget(), NotebookTab('Database', 'ʘ')
        )
        self._notebook.append_page(
            QueuePlaylistWidget(), NotebookTab('Playlist', '◎')
        )
        self._rules_page = RulesPage()
        self._notebook.append_page(
            self._rules_page, NotebookTab('Rules', '💡')
        )
        self._examine_page = ExamineSongPage()
        self._notebook.append_page(
            ExamineSongPage(), NotebookTab('Examine', '☑')
        )
        self._history_page = HistoryPage()
        self._notebook.append_page(
            self._history_page, NotebookTab('History', '⪜')
        )
        self._notebook.append_page(
            GraphPage(), NotebookTab('Graph', '☊')
        )

        self.show_all()

        self._last_song = None
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER:
            current_song_uri = None
            with g.client.lock_currentsong() as song:
                if song is not None:
                    current_song_uri = song.uri

            song_changed = self._last_song != current_song_uri

            listened_percent = g.heartbeat.last_listened_percent
            enough_listened = listened_percent >= SESSION.data.listen_threshold
            if song_changed and enough_listened:
                if self._last_song is not None:
                    SESSION.feed_history(
                        SESSION.mapping[:self._last_song]
                    )

            self._rules_page.update()
            self._history_page.update()
            self._notebook.show_all()

            self._last_song = current_song_uri


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
        action = self._actions[button]
        action()


class NaglfarWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.Window.__init__(self, title="Naglfar", application=app)
        self.set_default_size(1400, 900)

        # application icon:
        script_path = os.path.dirname(os.path.abspath(__file__))
        pixbuf_path = os.path.join(script_path, 'logo.png')
        pixbuf = GdkPixbuf.Pixbuf.new_from_file(pixbuf_path)
        self.set_icon(pixbuf)

        self._volume_slider = BarSlider()
        self._volume_slider.set_size_request(60, 25)

        volume_alignment = Gtk.Alignment()
        volume_alignment.set(0.5, 0.5, 0, 0)
        volume_alignment.add(self._volume_slider)

        g.client.signal_add('client-event', self._on_client_event)
        self._volume_slider.connect(
            'percent-change', self._on_volume_click_event
        )

        self._progress_bar = ProgressSlider()
        self._progress_bar.set_size_request(200, 15)

        progress_alignment = Gtk.Alignment()
        progress_alignment.set(0.5, 0.5, 0, 0)
        progress_alignment.add(self._progress_bar)

        GLib.timeout_add(500, self._timeout_callback)
        self._progress_bar.connect('percent-change', self._on_percent_change)

        app_box = Gtk.VBox()
        self._headerbar = Gtk.HeaderBar()
        self._headerbar.pack_start(PlaybuttonBox())
        self._headerbar.pack_start(progress_alignment)
        self._headerbar.pack_end(volume_alignment)
        self._headerbar.pack_end(ModebuttonBox())
        self._headerbar.set_show_close_button(True)
        app_box.pack_start(self._headerbar, False, False, 0)

        # a scrollbar for the child widget (that is going to be the textview)
        scrolled_window = Gtk.ScrolledWindow()
        container = NaglfarContainer()
        scrolled_window.add(container)

        app_box.pack_start(scrolled_window, True, True, 0)
        self.add(app_box)

    def _on_client_event(self, client, event):
        if event & Idle.MIXER:
            with client.lock_status() as status:
                if status is not None:
                    self._volume_slider.percent = status.volume / 100

        if event & Idle.PLAYER:
            with client.lock_currentsong() as song:
                if song is not None:
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
        win = NaglfarWindow(self)
        win.connect('delete-event', self.do_close_application)
        win.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_close_application(self, window, event):
        SESSION.save()
        window.destroy()
        shutdown_application()

if __name__ == '__main__':
    if '--help' in sys.argv:
        print('{} [music_dir_root]'.format(sys.argv[0]))
        sys.exit(0)

    # sometimes pickle.dump hits max recursion depth...
    sys.setrecursionlimit(sys.getrecursionlimit() * 2)

    app = NaglfarApplication()
    exit_status = app.run(sys.argv[1:])
    sys.exit(exit_status)
