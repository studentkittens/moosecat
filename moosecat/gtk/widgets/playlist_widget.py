from queue import Queue, Empty
from gi.repository import Gtk, GLib
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.widgets import PlaylistTreeModel


import logging
LOGGER = logging.getLogger('playlist_widget')


class PlaylistWidget(Gtk.VBox):
    def __init__(self, col_names=('Artist', 'Album', 'Title')):
        Gtk.Box.__init__(self)
        self._scw = Gtk.ScrolledWindow()
        self._scw.set_policy(
                Gtk.PolicyType.AUTOMATIC,
                Gtk.PolicyType.AUTOMATIC
        )

        self._model = PlaylistTreeModel([])

        # GtkSearchEntry configuration
        self._entry = Gtk.SearchEntry()
        self._entry.connect('changed', self._on_entry_changed)

        # Treeview Configuration
        self._view = Gtk.TreeView(model=None)
        self._view.set_fixed_height_mode(True)
        self._view.set_rules_hint(True)
        self._view.connect('row-activated', self._on_row_activated)

        selection = self._view.get_selection()
        selection.set_mode(Gtk.SelectionMode.MULTIPLE)

        for i, text in enumerate(col_names):
            renderer = Gtk.CellRendererText()
            renderer.set_fixed_height_from_font(1)

            col = Gtk.TreeViewColumn(text, renderer, text=i)
            col.set_min_width(250)
            col.set_sizing(Gtk.TreeViewColumnSizing.FIXED)
            self._view.append_column(col)

        self._scw.add(self._view)
        self.pack_start(self._scw, True, True, 1)
        self.pack_start(self._entry, False, False, 1)
        self.pack_start(Gtk.HSeparator(), False, False, 1)

        # Typing Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        # Fill it initially with the full Queue
        self._search_queue.put('*')
        GLib.timeout_add(150, self._internal_search)

        self.show_all()

    def set_model(self, model):
        self._model = model
        self._view.set_model(model)

    ############################
    #  Private Helper Methods  #
    ############################

    def _set_entry_icon(self, stock_name, primary=False):
        position = Gtk.EntryIconPosition.PRIMARY if primary else Gtk.EntryIconPosition.SECONDARY
        self._entry.set_icon_from_stock(position, stock_name)

    def _check_query_syntax(self, query_text):
        try:
            LOGGER.debug('Compiled Query: ' + parse_query(query_text))
            return None, -1
        except QueryParseException as error:
            logging.debug('Invalid Query Syntax: ' + str(error.msg))
            return error.msg, error.pos

    def _internal_search(self):
        try:
            query_text = None
            while True:
                query_text = self._search_queue.get_nowait()
        except Empty:
            if query_text is not None:
                msg, pos = self._check_query_syntax(query_text)
                if msg is not None:
                    user_message = 'around char {pos}: {msg}'.format(pos=pos, msg=msg)
                    self._set_entry_icon('gtk-dialog-error', primary=False)
                    self._entry.set_icon_tooltip_text(
                            Gtk.EntryIconPosition.SECONDARY,
                            user_message
                    )
                else:
                    # self._set_entry_icon('gtk-dialog-info', primary=False)
                    self._entry.set_icon_tooltip_text(
                            Gtk.EntryIconPosition.SECONDARY,
                            'Waiting for input.'\
                    )

                try:
                    self.do_search(query_text)
                except:
                    LOGGER.exception('Error while do_search')
            return True

    ######################
    #  Subclass Methods  #
    ######################

    def do_search(self, query):
        raise NotImplementedError(
                'PlaylistWidget subclasses should implement do_search()'
        )

    def do_row_activated(self, row):
        raise NotImplementedError(
                'PlaylistWidget subclasses should implement do_row_activated()'
        )

    ###########################
    #  Signal Implementation  #
    ###########################

    def _on_entry_changed(self, entry):
        self._search_queue.put(entry.get_text())

    def _on_row_activated(self, view, path, tv_column):
        print(self._model.data_from_path(path))
        self.do_row_activated(self._model.data_from_path(path))
