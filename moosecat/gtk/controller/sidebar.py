from moosecat.boot import g
from gi.repository import Gtk, Gdk, GdkPixbuf
from moosecat.gtk.interfaces import Hideable

# Custom Cell Renderer for the Browser.
from moosecat.gtk.widgets import CellRendererArrow


class Sidebar(Hideable):
    def __init__(self, builder):
        self._builder = builder

        self._browser_widgets = {}
        self._treeview = builder.get_object('plugin_view')
        self._main_pane = builder.get_object('main_pane')
        self._store = Gtk.ListStore(str, str)

        # Make the treeview integrate better in the rest of the application
        color = self._main_pane.get_style_context().get_background_color(Gtk.StateFlags.NORMAL)
        self._treeview.override_background_color(
                Gtk.StateFlags.NORMAL,
                Gdk.RGBA(color.red, color.green, color.blue)
        )

        self._treeview.append_column(Gtk.TreeViewColumn('Browser', CellRendererArrow(), text=0, icon=1))
        self._treeview.set_headers_visible(False)
        self._treeview.set_model(self._store)
        self._treeview.get_selection().connect('changed', self.on_selection_changed)

        # Fill something into the browser list
        self.collect_browsers()

        # Select the first one
        path_to_first = Gtk.TreePath.new_from_string('0')
        self._treeview.get_selection().select_path(path_to_first)

    def collect_browsers(self):
        # Get a List of Gtk Browsers
        browsers = g.psys.category('GtkBrowser')
        browsers.sort(key=lambda b: b.priority(), reverse=True)

        for browser in browsers:
            # Tell them to build their GUI up.
            browser.do_build()

            # Render the icon in the style of treeview
            icon = browser.get_browser_icon_name()

            # Name right side of Icon
            display_name = browser.get_browser_name()

            # Actually append them to the list
            self._store.append((display_name, icon))

            # Remember the widget
            self._browser_widgets[display_name] = browser.get_root_widget()

    ########################
    #  Hideable Interface  #
    ########################

    def get_hide_names(self):
        return ['sidebar']

    def get_hide_widgets(self):
        return [self._treeview]

    #############
    #  Signals  #
    #############

    def on_selection_changed(self, selection):
        (model, iterator) = selection.get_selected()

        widget = self._main_pane.get_child()
        if widget is not None:
            self._main_pane.remove(widget)

        self._main_pane.add(self._browser_widgets[model[iterator][0]])
        self._main_pane.show_all()
