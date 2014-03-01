#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk


class NotebookTab(Gtk.Box):
    """Label of a GtkNotebook Tab. Shows a unicode glyph symbol and a text.
    """
    def __init__(self, label, symbol='<big><b>♬</b></big> '):
        Gtk.Box.__init__(self, Gtk.Orientation.HORIZONTAL)

        # A nice music note at the left for some visual candy
        icon = Gtk.Label()
        icon.set_use_markup(True)
        icon.set_markup('<big><big>' + symbol + '</big></big> ')

        # Pack them in the container box
        self.pack_start(icon, True, True, 0)
        self.pack_start(Gtk.Label(label), True, True, 0)
        self.show_all()


class SlideNotebook(Gtk.Box):
    """
    A Notebook-like widget that adds effects when switching the page.

    Internally a GtkStack is used. This class is not compatible with
    Gtk.Notebook, only 2 methods are implemented, which aren't compatible
    themselves:

        - append_page(name, widget)
        - set_action_widget(widget)

    This class derives from Gtk.Box.
    """
    def __init__(self, transition_time=300, trans_type=Gtk.StackTransitionType.SLIDE_LEFT_RIGHT):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.VERTICAL)

        self._notebook = Gtk.Notebook()
        self._notebook.set_show_border(False)
        self._notebook.connect('switch-page', self._on_switch_page)
        self._stack = Gtk.Stack()
        self._stack.set_transition_type(trans_type)
        self._stack.set_transition_duration(transition_time)
        self._widget_table = {}

        self.pack_start(self._notebook, False, False, 0)
        self.pack_start(self._stack, True, True, 0)
        self.show_all()

    def append_page(self, name, widget, symbol='<big><b>♬</b></big> '):
        dummy_box = Gtk.EventBox()
        self._widget_table[dummy_box] = name
        self._stack.add_named(widget, name)
        self._notebook.append_page(dummy_box, NotebookTab(name, symbol))

    def set_action_widget(self, widget):
        self._notebook.set_action_widget(widget, Gtk.PackType.END)

    def _on_switch_page(self, nb, page_widget, page_num):
        name = self._widget_table[page_widget]
        self._stack.set_visible_child_name(name)


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main() as win:
        nb = SlideNotebook()
        nb.append_page('one', Gtk.Button('1'))
        nb.append_page('two', Gtk.Button('2'))
        nb.append_page('three', Gtk.Button('3'))
        win.add(nb)
