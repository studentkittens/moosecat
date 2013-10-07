from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk
from moosecat.boot import g


class InfoBar(Hideable):
    def __init__(self, builder):
        self._bar = builder.get_object('infobar')
        self._box = builder.get_object('infobar_box')
        self._bar.connect('response', self._on_response)
        self._bar.add_button('OK?', 1)
        g.client.signal_add('logging', self._on_log_event)
        self._bar.response(1)

    def _on_response(self, info_bar, response_id):
        print('stuff')
        info_bar.hide()

    def _on_log_event(self, client, msg, level):
        message_type = {
            'critical' : Gtk.MessageType.ERROR,
            'error'    : Gtk.MessageType.ERROR,
            'warning'  : Gtk.MessageType.WARNING,
            'info'     : Gtk.MessageType.INFO,
            'debug'    : Gtk.MessageType.OTHER
        }.get(level, Gtk.MessageType.ERROR)

        if message_type < Gtk.MessageType.WARNING:
            return

        self._bar.set_message_type(message_type)
        content_area = self._bar.get_content_area()

        # Clear previous widgets in content area
        for widget in content_area.get_children():
            content_area.remove(widget)

        content_area.add(Gtk.Label(msg))
        self._bar.show_all()
