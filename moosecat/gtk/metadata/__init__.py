#!/usr/bin/env python
# encoding: utf-8


if __name__ == '__main__':
    from gi.repository import Gtk

    from moosecat.gtk.runner import main
    from moosecat.gtk.metadata.container import MetadataChooser
    from moosecat.gtk.metadata.settings import SettingsChooser

    settings = Gtk.Settings.get_default()
    settings.set_property('gtk-application-prefer-dark-theme', True)

    with main(metadata=True) as win:
        settings_chooser = SettingsChooser()

        scw = Gtk.ScrolledWindow()
        scw.add(settings_chooser)
        scw.set_size_request(400, -1)

        chooser = MetadataChooser(
            get_type='cover',
            artist='Knorkator',
            album='Hasenchartbreaker'
        )

        chooser.connect(
            'get-type-changed',
            lambda _, get_type: settings_chooser.update_provider(get_type)
        )

        settings_chooser.connect(
            'view-all-in-database',
            lambda _, get_type_only, amount: chooser.show_all_in_database(
                get_type_only,
                amount
            )
        )

        settings_chooser.update_provider(chooser.get_selected_type())

        paned = Gtk.Paned()
        paned.pack1(chooser, True, False)
        paned.pack2(scw, False, True)

        win.set_default_size(1100, 900)
        win.add(paned)
