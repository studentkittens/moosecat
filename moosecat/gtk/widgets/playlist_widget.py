from gi.repository import Gtk, Gdk
from moosecat.core import parse_query, QueryParseException
from moosecat.gtk.widgets import PlaylistTreeModel
from moosecat.gtk.widgets.action_dialog import create_action_popover
from moosecat.gtk.utils import add_keybindings


import logging
LOGGER = logging.getLogger('playlist_widget')


def make_configure_row(label, widget):
    label_widget = Gtk.Label()
    label_widget.set_use_markup(True)
    label_widget.set_markup('<b>' + label + '</b>')
    label_widget.set_halign(Gtk.Align.START)

    widget.set_halign(Gtk.Align.END)

    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
    box.pack_start(label_widget, True, True, 0)
    box.pack_start(widget, False, False, 0)
    box.set_hexpand(True)
    box.set_halign(Gtk.Align.FILL)
    return box


class ColumnConfigurator(Gtk.ListBox):
    def __init__(self):
        Gtk.ListBox.__init__(self)


class PlaylistWidget(Gtk.ScrolledWindow):
    def __init__(self, col_names=(('Artist', 150, 1.0), ('Album', 200, 0.0), ('Title', 250, 1.0))):
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
        # self._view.set_rules_hint(True)
        self._view.set_enable_search(False)
        self._view.set_search_column(-1)

        self._view.connect('row-activated', self._on_row_activated)
        self._view.connect('button-press-event', self._on_button_press_event)

        add_keybindings(self._view, {
            '<Shift>space': lambda view, key: self.jump_to_selection()
        })

        selection = self._view.get_selection()
        selection.set_mode(Gtk.SelectionMode.MULTIPLE)

        for i, (text, width, align) in enumerate(col_names):
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
                renderer.set_alignment(align, 0.5)
                col = Gtk.TreeViewColumn(text, renderer, text=i)

            col.set_alignment(align)
            col.set_min_width(width)
            col.set_sizing(Gtk.TreeViewColumnSizing.FIXED)
            col.set_clickable(True)
            col.connect('clicked', self.on_column_clicked)
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

    def on_column_clicked(self, column):
        scale = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL,
            0.0, 1.0, 0.05
        )
        scale.set_size_request(170, -1)

        entry = Gtk.Entry()
        entry.set_size_request(170, -1)

        grid = Gtk.Grid()
        grid.set_border_width(5)
        grid.attach(make_configure_row('Name:', entry), 0, 0, 1, 1)
        grid.attach(make_configure_row('Alignment:', scale), 0, 1, 1, 1)

        popover = create_action_popover(
            column.get_button(), grid,
            'Configure Column'
        )
        popover.show_all()

    def _on_row_activated(self, view, path, tv_column):
        self.do_row_activated(self._model.data_from_path(path))

    def _on_button_press_event(self, treeview, event):
        if self._menu is not None:
            if event.type == Gdk.EventType.BUTTON_PRESS and event.button == 3:
                self._menu.simple_popup(event)
                return True
        return False
