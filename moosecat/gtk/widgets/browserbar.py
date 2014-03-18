#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import logging

# Internal:
from moosecat.boot import g
from moosecat.gtk.widgets import SlideNotebook
from moosecat.gtk.browser import DEFAULT_BROWSERS_CLASSES


LOGGER = logging.getLogger('BrowserBar')


class BrowserBar(SlideNotebook):
    def __init__(self):
        # TODO: read config values
        SlideNotebook.__init__(self)

        # Make switching to certain browser for others possible:
        g.register('gtk_sidebar', self)

        # Fill something into the browser list
        self.collect_browsers()

    def collect_browsers(self):
        # Get a List of Gtk Browsers
        browsers = [browser () for browser in DEFAULT_BROWSERS_CLASSES]
        # browsers = g.psys.category('GtkBrowser')
        # browsers.sort(key=lambda b: b.priority(), reverse=True)

        for browser in browsers:
            # Tell them to build their GUI up.
            browser.do_build()

            # Render the icon in the style of treeview
            icon = browser.get_browser_icon_name()

            # Name right side of Icon
            display_name = browser.get_browser_name()

            # Actually append them to the list
            self.append_page(display_name, browser.get_root_widget(), symbol=icon)

    def switch_to(self, browser_name):
        self.set_visible_page(browser_name)


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main(store=True) as win:
        win.add(BrowserBar())
