#!/usr/bin/env python
# encoding: utf-8


class GtkBrowser:
    '''
    Implement this interface to be added in the GTK's UI as a Browser.
    '''
    def do_build(self):
        pass

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        pass

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        pass

    def get_browser_icon_name(self):
        '''
        Get the icon-name to be displayed along the name.
        For example: GTK.STOCK_CDROM.
        '''
        pass


from .database import DatabaseBrowser
from .queue import QueueBrowser
from .settings import SettingsBrowser
from .stored_playlist import StoredPlaylistBrowser

DEFAULT_BROWSERS_CLASSES = [
    DatabaseBrowser,
    QueueBrowser,
    SettingsBrowser,
    StoredPlaylistBrowser
]
