from gi.repository import Gtk, GLib, Gdk
from logging import getLogger


LOGGER = getLogger('GtkKeyHandler')
HANDLERS = []


class KeyHandler:
    def __init__(self, widget, key_dict):
        self._widget, self._key_dict = widget, {}
        for key_descr, callback in key_dict.items():
            if not callable(callback):
                raise ValueError(str(callback) + ' is not callable.')
            self._key_dict[parse_key_descr(key_descr)] = callback

        self._bad_state_mask = 0       | \
            Gdk.ModifierType.MOD1_MASK | \
            Gdk.ModifierType.MOD2_MASK | \
            Gdk.ModifierType.MOD3_MASK | \
            Gdk.ModifierType.MOD4_MASK | \
            Gdk.ModifierType.MOD5_MASK

    def handle_key_press(self, widget, event):
        state = Gdk.ModifierType(event.state & ~self._bad_state_mask)
        lower_key = Gdk.keyval_to_lower(event.keyval)
        callback = self._key_dict.get((lower_key, state))
        if callback is not None:
            LOGGER.debug('Keypress ' + event.string + ' on ' + str(widget))
            rc = callback(self._widget, event)
            return bool(rc) if rc is not None else True
        return False


def parse_key_descr(descr):
    return Gtk.accelerator_parse(descr)


def add_keybindings(widget, key_dict, enable_extra_events=False):
    if enable_extra_events:
        widget.add_events(
                Gdk.EventMask.BUTTON_PRESS_MASK |
                Gdk.EventMask.BUTTON_RELEASE_MASK
        )

    handler = KeyHandler(widget, key_dict)
    widget.connect('key-press-event', handler.handle_key_press)
    HANDLERS.append(handler)


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
            btn = Gtk.Button('Press something on me')
            ent = Gtk.Entry()

            box = Gtk.Box()
            box.set_orientation(Gtk.Orientation.VERTICAL)
            box.pack_start(btn, True, True, 1)
            box.pack_start(ent, False, False, 1)

            add_keybindings(win, {
                '<Ctrl>q': lambda win, key: self.quit(),
                '<Ctrl>w': lambda win, key: win.hide()
            })
            add_keybindings(btn, {
                '<Shift>p': lambda btn, key: btn.clicked()
            })
            add_keybindings(ent, {
                'Escape': lambda ent, key: btn.grab_focus()
            })

            win.add(box)
            win.show_all()

        def do_startup(self):
            # start the application
            Gtk.Application.do_startup(self)

    app = MyApplication()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
