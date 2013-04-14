from gi.repository import Gtk
from moosecat.plugins import IGtkBrowser


class QueueBrowser(IGtkBrowser):
    def do_build(self):
        self._label = Gtk.Label("Oh, well.")

    #######################
    #  IGtkBrowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._label

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Settings'

    def get_browser_icon_name(self):
        return Gtk.STOCK_PREFERENCES
