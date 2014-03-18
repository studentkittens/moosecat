#!/usr/bin/env python
# encoding: utf-8


if __name__ == '__main__':
    from gi.repository import Gtk

    from moosecat.gtk.runner import main
    from moosecat.gtk.metadata.container import MetadataChooser
    from moosecat.gtk.metadata.settings import SettingsChooser

    with main(metadata=True) as win:
        scw = Gtk.ScrolledWindow()
        scw.add(SettingsChooser())
        scw.set_size_request(400, -1)

        chooser = MetadataChooser(
            get_type='cover',
            artist='Knorkator',
            album='Hasenchartbreaker'
        )

        paned = Gtk.Paned()
        paned.pack1(chooser, True, False)
        paned.pack2(scw, False, True)

        win.set_default_size(1100, 900)
        win.add(paned)
