#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, GObject


class NotebookTab(Gtk.Box):
    '''
    A nicely to use tab label that contains an icon, a text and a close button.
    If the user presses the close button the tab-close signal is emitted,
    to which the page_num is passed.

    You have to keep the page_nums updated yourself by calling set_page_num()
    when deleting pages in the middle.

    Future version might also support an editable text entry
    '''
    # Signal that is triggered when closing the tab.
    __gsignals__ = {
            'tab-close': (
                    GObject.SIGNAL_RUN_FIRST,  # Run-Order
                    None,                      # Return Type
                    (int, )                    # Parameters
            )
    }

    def __init__(self, playlist_name, page_num, icon_txt='<big><b>♬</b></big> '):
        '''
        Create a new NotebookTab label.

        :param playlist_name: The name of the playlist to show. Can be any string really.
        :param page_num: The page number (passed to the tab-close signal)
        '''
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)
        self._playlist_name = playlist_name
        self._page_num = page_num

        # A nice music note at the left for some visual candy
        self._icon = Gtk.Label()
        self._icon.set_markup(icon_txt)
        self._icon.set_use_markup(True)

        # The main text.
        self._label = Gtk.Label(playlist_name)

        # Make a nice little closebutton that that displays a "x"
        self._close_button = Gtk.Button('x')
        self._close_button.set_relief(Gtk.ReliefStyle.NONE)
        self._close_button.set_focus_on_click(False)
        self._close_button.connect(
                'clicked',
                lambda _: self.emit('tab-close', self._page_num)
        )

        # Pack them in the container box
        for widget in (self._icon, self._label, self._close_button):
            self.pack_start(widget, True, True, 0)
        self.show_all()

    def get_playlist_name(self):
        'Return the playlisy name passed to the constructor'
        return self._playlist_name

    def set_page_num(self, page_num):
        'Set the current page num (forwared to the tab-close callback)'
        self._page_num = page_num
