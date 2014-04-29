from gi.repository import Gtk, Gdk
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.widgets import PlaylistTreeModel
from moosecat.gtk.utils import add_keybindings


import logging
LOGGER = logging.getLogger('playlist_widget')


class PlaylistWidget(Gtk.ScrolledWindow):
    def __init__(self, col_names=(('Artist', 150), ('Album', 200), ('Title', 250))):
        Gtk.ScrolledWindow.__init__(self)
        self.set_policy(
            Gtk.PolicyType.AUTOMATIC,
            Gtk.PolicyType.AUTOMATIC
        )

        self._menu = None
        self._model = PlaylistTreeModel([])

        # Treeview Configuration
        self._view = Gtk.TreeView(model=None)
        self._view.set_fixed_height_mode(True)
        self._view.set_rules_hint(True)
        self._view.connect('row-activated', self._on_row_activated)
        self._view.connect('button-press-event', self._on_button_press_event)

        add_keybindings(self._view, {
            '<Shift>space': lambda view, key: self.jump_to_selection()
        })

        selection = self._view.get_selection()
        selection.set_mode(Gtk.SelectionMode.MULTIPLE)

        for i, (text, width) in enumerate(col_names):
            # TODO: Think of a clearer API
            if '<pixbuf>' in text:
                _, header = text.split(':', maxsplit=1)
                renderer = Gtk.CellRendererPixbuf()
                col = Gtk.TreeViewColumn(header)
                col.pack_start(renderer, False)
                col.add_attribute(renderer, 'stock-id', i)
            elif '<progress>' in text:
                _, header = text.split(':', maxsplit=1)
                col = Gtk.TreeViewColumn(header)
                renderer = Gtk.CellRendererProgress()
                col.pack_start(renderer, False)
                col.add_attribute(renderer, 'text', i)
                col.add_attribute(renderer, 'value', i)
            else:
                renderer = Gtk.CellRendererText()
                renderer.set_fixed_height_from_font(1)
                col = Gtk.TreeViewColumn(text, renderer, text=i)

            col.set_min_width(width)
            col.set_sizing(Gtk.TreeViewColumnSizing.FIXED)
            self._view.append_column(col)

        # Fill it initially with the full Queue
        self.add(self._view)
        self.show_all()

    def set_model(self, model):
        self._model = model
        self._view.set_model(model)

    def set_menu(self, menu):
        self._menu = menu

    def get_selected_rows(self):
        model, paths = self._view.get_selection().get_selected_rows()
        return model, (model[path] for path in paths)

    def jump_to_selection(self):
        rows = self._view.get_selection().get_selected_rows()
        if rows:
            self._view.scroll_to_cell(rows[int(len(rows) / 2)], None, False, 0, 0)
        return True

    ############################
    #  Private Helper Methods  #
    ############################

    def _check_query_syntax(self, query_text):
        try:
            LOGGER.debug('Compiled Query: ' + parse_query(query_text))
            return None, -1
        except QueryParseException as error:
            logging.debug('Invalid Query Syntax: ' + str(error.msg))
            return error.msg, error.pos

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

    def _on_row_activated(self, view, path, tv_column):
        self.do_row_activated(self._model.data_from_path(path))

    def _on_button_press_event(self, treeview, event):
        if self._menu is not None:
            if event.type == Gdk.EventType.BUTTON_PRESS and event.button == 3:
                self._menu.simple_popup(event)
                return True
        return False
