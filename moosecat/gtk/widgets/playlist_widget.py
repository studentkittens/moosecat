from queue import Queue, Empty
from gi.repository import Gtk, GLib, Gdk
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.widgets import PlaylistTreeModel
from moosecat.gtk.utils import add_keybindings


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

        self._menu = None
        self._model = PlaylistTreeModel([])

        # GtkSearchEntry configuration
        self._entry = Gtk.SearchEntry()
        self._entry.connect('changed', self._on_entry_changed)

        # Treeview Configuration
        self._view = Gtk.TreeView(model=None)
        self._view.set_fixed_height_mode(True)
        self._view.set_rules_hint(True)
        self._view.connect('row-activated', self._on_row_activated)
        self._view.connect('button-press-event', self._on_button_press_event)

        add_keybindings(self._view, {
            '<Shift>space': lambda view, key: self.jump_to_selection(),
            '<Ctrl>f':      lambda view, key: self.focus_searchbar(),
            '/':            lambda view, key: self.focus_searchbar()
        })

        add_keybindings(self._entry, {
            'Escape': lambda ent, key: self.focus_treeview()
        })

        selection = self._view.get_selection()
        selection.set_mode(Gtk.SelectionMode.MULTIPLE)

        for i, text in enumerate(col_names):
            # TODO: Think of a clearer API
            if '<pixbuf>' in text:
                _, header = text.split(':', maxsplit=1)
                renderer = Gtk.CellRendererPixbuf()
                col = Gtk.TreeViewColumn(header)
                col.pack_start(renderer, False)
                col.add_attribute(renderer, 'stock-id', i)
                col.set_min_width(30)
            else:
                renderer = Gtk.CellRendererText()
                renderer.set_fixed_height_from_font(1)
                col = Gtk.TreeViewColumn(text, renderer, text=i)
                col.set_min_width(250)
            col.set_sizing(Gtk.TreeViewColumnSizing.FIXED)
            self._view.append_column(col)

        self._scw.add(self._view)
        self.pack_start(self._scw, True, True, 1)

        self._revealer = Gtk.Revealer()
        self._revealer.add(self._entry)
        self.pack_start(self._revealer, False, False, 1)
        self.pack_start(Gtk.HSeparator(), False, False, 1)

        # Typing Optimzations
        self._last_length = -1
        self._search_queue = Queue()

        # Fill it initially with the full Queue
        self._search_queue.put('*')
        GLib.timeout_add(150, self._internal_search)

        self.show_all()
        self.focus_searchbar()

    def focus_searchbar(self):
        self._revealer.show_all()
        self._entry.grab_focus()

    def focus_treeview(self):
        self._revealer.hide()
        self._view.grab_focus()

    def set_model(self, model):
        self._model = model
        self._view.set_model(model)

    def set_menu(self, menu):
        self._menu = menu

    def jump_to_selection(self):
        print('JUMPDAFUCKUP')
        rows = self._view.get_selection().get_selected_rows()
        if rows:
            self._view.scroll_to_cell(rows[int(len(rows) / 2)], None, False, 0, 0)
            print(rows)
        return True

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

    def _on_button_press_event(self, treeview, event):
        if self._menu is not None:
            if event.type == Gdk.EventType.BUTTON_PRESS and event.button == 3:
                self._menu.simple_popup(event)
                return True
        return False
