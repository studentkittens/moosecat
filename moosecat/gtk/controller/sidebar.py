from moosecat.boot import g
from moosecat.gtk.interfaces import Hideable

# Custom Cell Renderer for the Browser.
from moosecat.gtk.widgets import CellRendererArrow

from gi.repository import GLib, Gdk, Gtk, GtkClutter, Clutter
from sys import argv
from logging import getLogger


LOGGER = getLogger('GtkSidebar')
CLUTTER_IS_INITIALIZED = False


def _render_pixbuf(widget, width, height):
    # Use an OffscreenWindow to render the widget
    off_win = Gtk.OffscreenWindow()
    off_win.add(widget)
    off_win.set_size_request(width, height)
    off_win.show_all()

    # this is needed, otherwise the screenshot is black
    while Gtk.events_pending():
        Gtk.main_iteration()

    # Render the widget to a GdkPixbuf
    widget_pix = off_win.get_pixbuf()

    # Remove the widget again (so we can reparent it later)
    off_win.remove(widget)

    return widget_pix


def _create_texture(pixbuf, width, height, x_off=0, y_off=0):
    # Create a Clutter Texture from it
    ctex = GtkClutter.Texture()
    if pixbuf is not None:
        ctex.set_from_pixbuf(pixbuf)

    # Set some dimensions
    if x_off is not 0:
        ctex.set_x(x_off)

    if y_off is not 0:
        ctex.set_y(y_off)

    ctex.set_width(width)
    ctex.set_height(height)

    return ctex


def swipe(parent, old_widget, new_widget, right=True, time_ms=750):
    if not CLUTTER_IS_INITIALIZED:
        global CLUTTER_IS_INITIALIZED
        CLUTTER_IS_INITIALIZED = True
        GtkClutter.init(argv)

    # Allocation of the old widget.
    # This is also used to determine the size of the new_widget
    alloc = old_widget.get_allocation()
    width, height = alloc.width, alloc.height

    # Get the Imagedate of the old widget
    old_pixbuf = Gdk.pixbuf_get_from_window(
            old_widget.get_window(), 0, 0, width, height
    )
    #old_pixbuf = _render_pixbuf(old_widget, width, height)

    # Render the new image (which is not displayed yet)
    new_pixbuf = _render_pixbuf(
            new_widget, width, height
    )

    # Toplevel container inside the stage
    container = Clutter.Actor()

    # Left-Side Image
    container.add_child(_create_texture(
        old_pixbuf, width, height, x_off=-width if not right else 0)
    )

    # Right-Side Image
    container.add_child(_create_texture(
        new_pixbuf, width, height, x_off=0 if not right else +width)
    )

    # Shuffle widgets around
    embed = GtkClutter.Embed()
    embed.get_stage().add_child(container)
    parent.remove(old_widget)
    parent.add(embed)
    parent.show_all()

    # Actual animation:
    transition = Clutter.PropertyTransition()
    transition.set_property_name('x')
    transition.set_duration(time_ms)
    transition.set_progress_mode(Clutter.AnimationMode.EASE_IN_OUT_EXPO)
    transition.set_from(0)

    if right is True:
        transition.set_to(-width)
    else:
        transition.set_to(+width)

    # Now go back to normal and swap clutter with the new widget
    def restore_widget(*args):
        parent.remove(embed)
        parent.add(new_widget)
        parent.show_all()

    # Be notified when animation ends
    transition.connect('completed', restore_widget)

    # Play the animation
    container.add_transition('animate-x', transition)


class Sidebar(Hideable):
    def __init__(self, builder):
        self._builder = builder

        self._browser_widgets = {}
        self._treeview = builder.get_object('plugin_view')
        self._main_pane = builder.get_object('main_pane')
        self._sidebar_box = builder.get_object('sidebar_box')
        self._store = Gtk.ListStore(str, str)

        self._main_align = builder.get_object('main_align')

        # Make the treeview integrate better in the rest of the application
        color = self._main_pane.get_style_context().get_background_color(Gtk.StateFlags.NORMAL)
        # self._treeview.override_background_color(
        #         Gtk.StateFlags.NORMAL,
        #         Gdk.RGBA(color.red, color.green, color.blue)
        # )

        self._treeview.append_column(
            Gtk.TreeViewColumn('Browser', CellRendererArrow(), text=0, icon=1)
        )
        self._treeview.set_headers_visible(False)
        self._treeview.set_model(self._store)
        self._treeview.get_selection().connect('changed', self._on_selection_changed)

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

    def _switch_widgets(self, new_widget):
        widget = self._main_align.get_child()

        if g.config.get('gtk.swipe_effect_enabled'):
            if widget is not None:
                swipe(self._main_align, widget, new_widget)
            else:
                self._main_align.add(new_widget)
        else:
            if widget is not None:
                self._main_align.remove(widget)
            self._main_align.add(new_widget)
        self._main_align.show_all()

    def switch_to(self, browser_name):
        for idx, (name, _) in enumerate(self._store):
            if name == browser_name:
                # Triggers _on_selection_changed, which changes the browser
                self._treeview.get_selection().select_path(Gtk.TreePath(str(idx)))
                break
        else:
            raise ValueError('No such registered Browser: ' + browser_name)

    def toggle_sidebar_visibility(self):
        if self._sidebar_box.is_visible():
            self._sidebar_box.hide()
        else:
            self._sidebar_box.show_all()

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

    def _on_selection_changed(self, selection):
        (model, iterator) = selection.get_selected()
        self._switch_widgets(self._browser_widgets[model[iterator][0]])
