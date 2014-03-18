#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
from contextlib import contextmanager
import logging

# Internal:
from moosecat.boot import boot_base, boot_store, boot_metadata, shutdown_application

# External:
from gi.repository import Gtk


@contextmanager
def main(store=False, metadata=False):
    win = Gtk.Window()

    # Connect to the test-server
    boot_base(
        verbosity=logging.DEBUG,
        protocol_machine='idle',
        host='localhost',
        port=6601
    )

    if store:
        boot_store()

    if metadata:
        boot_metadata()

    win.connect('destroy', Gtk.main_quit)

    try:
        yield win
    except:
        raise
    else:
        win.show_all()
        Gtk.main()
    finally:
        shutdown_application()
