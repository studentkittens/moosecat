#!/usr/bin/env python
# encoding: utf-8


from gi.repository import Gtk, GLib

from moosecat.gtk.runner import main
from moosecat.gtk.metadata.settings import SettingsChooser

from moosecat.gtk.browser import GtkBrowser


def view_all_in_database(settings_chooser, get_type_only, amount, chooser):
    count = chooser.show_all_in_database(get_type_only, amount)
    settings_chooser.set_database_count(count)


class MetadataBrowser(Gtk.Paned, GtkBrowser):
    def __init__(self):
        Gtk.Paned.__init__(self)

    def do_build(self):
        # TODO:
        from moosecat.gtk.metadata.container import MetadataChooser

        settings_chooser = SettingsChooser()

        scw = Gtk.ScrolledWindow()
        scw.add(settings_chooser)
        scw.set_size_request(400, -1)

        chooser = MetadataChooser(
            get_type='cover',
            artist='Knorkator',
            album='Hasenchartbreaker',
            title="Ich bin ein ganz besond'rer Mann"
        )

        chooser.connect(
            'get-type-changed',
            lambda _, get_type: settings_chooser.update_provider(get_type)
        )
        chooser.connect(
            'toggle-settings',
            lambda _, toggled: self.set_settings_visible(toggled)
        )

        settings_chooser.connect(
            'view-all-in-database',
            view_all_in_database,
            chooser
        )
        settings_chooser.update_provider(chooser.get_selected_type())

        self.pack1(chooser, True, False)
        self.pack2(scw, False, True)

        GLib.idle_add(lambda: self.set_settings_visible(False))

    def set_settings_visible(self, visible):
        alloc = self.get_allocation()
        if not visible:
            self.set_position(alloc.width)
        else:
            self.set_position(alloc.width / 2)

    ###################
    #  Browser Stuff  #
    ###################

    def get_root_widget(self):
        return self

    def get_browser_name(self):
        return 'Metadata'

    def get_browser_icon_name(self):
        return 'media-optical'


if __name__ == '__main__':
    with main(metadata=True) as win:
        chooser = MetadataBrowser()
        chooser.do_build()
        win.set_default_size(1100, 900)
        win.add(chooser)
