# Mooscat stuff
from moosecat.core import parse_query, QueryParseException, Idle
from moosecat.plugins import IGtkBrowser
from moosecat.gtk.playlist_widget import PlaylistWidget

from gi.repository import Gtk, GObject


class NotebookTab(Gtk.Box):
    __gsignals__ = {
            'tab-close': (
                    GObject.SIGNAL_RUN_FIRST,  # Run-Order
                    None,                      # Return Type
                    (int, )                    # Parameters
            )
    }

    def __init__(self, label_markup, page_num, stock_id='gtk-justify-fill'):
        #self._icon = Gtk.Image.new_from_stock(stock_id, Gtk.IconSize.MENU)
        self._icon = Gtk.Label()
        self._icon.set_markup('<big><b>♬</b></big>')

        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)
        self._label = Gtk.Label()
        self._label.set_markup(label_markup)
        self._label_markup = label_markup

        self._page_num = page_num

        self._close_button = Gtk.Button('x')
        self._close_button.set_relief(Gtk.ReliefStyle.NONE)
        self._close_button.set_focus_on_click(False)
        self._close_button.connect(
                'clicked',
                lambda btn: self.emit('tab-close', self._page_num)
        )

        for widget in (self._icon, self._label, self._close_button):
            self.pack_start(widget, True, True, 0)

        self.show_all()

    @property
    def label_markup(self):
        return self._label_markup

    @label_markup.setter
    def label(self, new_label):
        self._label_markup = new_label
        self._label.set_markup(new_label)

    def set_page_num(self, page_num):
        self._page_num = page_num


class StoredPlaylistAddEntry(Gtk.Box):
    __gsignals__ = {
            'playlist-added': (
                    GObject.SIGNAL_RUN_FIRST,  # Run-Order
                    None,                      # Return Type
                    (str, )                    # Parameters
            )
    }

    def __init__(self, notebook_widget):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)
        self._notebook = notebook_widget

        self._add_button = Gtk.Button()
        self._add_button.set_image(
                Gtk.Image.new_from_stock('gtk-add', Gtk.IconSize.MENU)
        )
        self._add_button.set_relief(Gtk.ReliefStyle.NONE)
        self._add_button.set_focus_on_click(False)
        self._add_button.connect('clicked', self._on_add_button_clicked)
        self._add_button.set_sensitive(False)

        self._entry = Gtk.Entry()
        self._entry.set_placeholder_text('<Enter Playlist Name>')
        self._entry.connect('changed', self._on_entry_changed)

        self.pack_start(self._entry, True, True, 0)
        self.pack_start(self._add_button, True, False, 0)
        self.show_all()

    def _on_entry_changed(self, entry):
        names = []
        for page_num in range(self._notebook.get_n_pages()):
            child = self._notebook.get_nth_page(page_num)
            nbtab = self._notebook.get_tab_label(child)
            if hasattr(nbtab, 'label_markup'):
                names.append(nbtab.label_markup)

        entry_text = entry.get_text()
        is_sensitive = bool(entry_text) and entry_text not in names
        self._add_button.set_sensitive(is_sensitive)

    def _on_add_button_clicked(self, button):
        text = self._entry.get_text()
        if text:
            self.emit('playlist-added', text)
            self._add_button.set_sensitive(False)


class StoredPlaylistBrowser(IGtkBrowser):
    def do_build(self):
        self._notebook = Gtk.Notebook()
        add_entry = StoredPlaylistAddEntry(self._notebook)
        add_entry.connect('playlist-added', self._on_add_button_clicked)
        self._notebook.set_action_widget(add_entry, Gtk.PackType.END)

        # Start configuration
        self._empty_page_is_shown = True
        self._on_del_button_clicked(None, 0)

        g.client.signal_add('client-event', self._on_client_event)

    def get_n_pages(self):
        return self._notebook.get_n_pages()

    def _update_tab_numbers(self):
        for page_num in range(self._notebook.get_n_pages()):
            child = self._notebook.get_nth_page(page_num)
            nbtab = self._notebook.get_tab_label(child)
            nbtab.set_page_num(page_num)

    def _show_empty_playlists(self):
        self._empty_page_is_shown = True
        info_label = Gtk.Label()
        info_label.set_markup('''\
                <big><big>♫</big></big>\n\
                <b>No stored playlists!</b>\n\
                <small><i>(Use the <big>+</big> in the upper corner to add some!)</i></small>\
        ''')
        info_label.set_justify(Gtk.Justification.CENTER)
        info_label.show()

        self._notebook.append_page(info_label, Gtk.Label(' ☉ '))
        self._notebook.show_all()

    def _update_stored_playlists(self):
        for name in g.client.store.stored_playlist_names:
            self._on_add_button_clicked(None, name)

    #####################
    #  Signal Handlers  #
    #####################

    def _on_client_event(self, client, event):
        if event & Idle.STORED_PLAYLIST:
            self._update_stored_playlists()

    def _on_add_button_clicked(self, _, playlist_name):
        if self._empty_page_is_shown is True:
            self._notebook.remove_page(0)
            self._empty_page_is_shown = False
        ntab = NotebookTab(playlist_name, page_num=self._notebook.get_n_pages())
        ntab.connect('tab-close', self._on_del_button_clicked)

        self._notebook.append_page(Gtk.Button('stuff.'), ntab)
        # self._notebook.append_page(PlaylistWidget(), ntab)
        self._notebook.show_all()

    def _on_del_button_clicked(self, _, page_num):
        self._notebook.remove_page(page_num)
        self._update_tab_numbers()
        if self._notebook.get_n_pages() is 0:
            self._show_empty_playlists()

    #######################
    #  IGtkBRowser Stuff  #
    #######################

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        return self._notebook

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        return 'Playlists'

    def get_browser_icon_name(self):
        return Gtk.STOCK_FIND_AND_REPLACE


if __name__ == '__main__':
    import sys
    from moosecat.core import Client

    c = Client()
    c.connect(port=6666)

    # Fake the g.client object.
    class G:
        pass
    g = G()
    g.client = c
    g.client.store_initialize('/tmp')

    class MyApplication(Gtk.Application):
        def __init__(self):
            Gtk.Application.__init__(self)

        def do_activate(self):
            spb = StoredPlaylistBrowser()
            spb.do_build()
            win = Gtk.ApplicationWindow(application=self)
            win.add(spb.get_root_widget())
            win.show_all()

        def do_startup(self):
            Gtk.Application.do_startup(self)

    app = MyApplication()
    exit_status = app.run(sys.argv)
    c.disconnect()
    sys.exit(exit_status)
