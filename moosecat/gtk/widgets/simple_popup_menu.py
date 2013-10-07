from gi.repository import Gtk, Gio, Gdk


class SimplePopupMenu(Gtk.Menu):
    '''
    Just a simpler version of Gtk.Menu with quicker to type methods.

    Otherwise it is a normal Gtk.Menu and can be used as such.
    '''
    def __init__(self):
        Gtk.Menu.__init__(self)

    def _add_item(self, item):
        self.append(item)
        self.show_all()

    def simple_add(self, name, callback=None, stock_id=None):
        '''Add a Gtk.MenuItem() with a certain name.

        :param callback: Callback that is called when the item is activated
        :param stock_id: Option stock_id to display along the name
        '''
        if stock_id is None:
            item = Gtk.MenuItem()
        else:
            item = Gtk.ImageMenuItem.new_from_stock(stock_id, None)

        item.set_label(name)
        if callable(callback):
            item.connect('activate', callback)
        self._add_item(item)

    def simple_add_checkbox(self, name, state=False, toggled=None):
        '''Add a Gtk.CheckMenuItem to the Menu with the initial *state* and
        the  callable *toggled* that is called when the state changes.
        '''
        item = Gtk.CheckMenuItem()
        item.set_label(name)
        if callable(toggled):
            item.connect('toggled', toggled)
        self._add_item(item)

    def simple_add_separator(self):
        '''Add a Gtk.SeparatorMenuItem to the Menu.
        '''
        self._add_item(Gtk.SeparatorMenuItem())

    def simple_popup(self, button_event):
        'A simpler version of Gtk.Menu.popup(), only requiring a GdkEventButton.'
        self.popup(None, None, None, None, button_event.button, button_event.time)


if __name__ == '__main__':
    import sys

    class MyWindow(Gtk.ApplicationWindow):
        def __init__(self, app):
            Gtk.Window.__init__(self, title="GMenu Example", application=app)

    class MyApplication(Gtk.Application):
        def __init__(self):
            Gtk.Application.__init__(self)

        def do_activate(self):
            win = MyWindow(self)
            btn = Gtk.Button('Right click me!')
            btn.connect('button-press-event', self.on_button_press_event)
            win.add(btn)
            win.show_all()

        def do_startup(self):
            # start the application
            Gtk.Application.do_startup(self)
            self.menu = SimplePopupMenu()
            self.menu.simple_add('About', self.about_cb)
            self.menu.simple_add('New', self.new_cb, stock_id='gtk-new')
            self.menu.simple_add_checkbox('Stateful', state=False, toggled=self.state_cb)
            self.menu.simple_add_separator()
            self.menu.simple_add('Quit', self.quit_cb)

        def on_button_press_event(self, button, event):
            # Check if right mouse button was preseed
            if event.type == Gdk.EventType.BUTTON_PRESS and event.button == 3:
                self.menu.simple_popup(event)
                return True  # event has been handled

        # callback function for "new"
        def new_cb(self, action):
            print("This does nothing. It is only a demonstration.")

        # callback function for "about"
        def about_cb(self, action):
            print("No AboutDialog for you. This is only a demonstration.")

        # callback function for "quit"
        def quit_cb(self, action):
            print("You have quit.")
            self.quit()

        def state_cb(self, menu_item):
            print(menu_item.get_active())

    app = MyApplication()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
