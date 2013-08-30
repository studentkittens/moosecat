from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.utils import plyr_cache_to_pixbuf
from moosecat.metadata import configure_query
from moosecat.core import Idle, Status
from moosecat.boot import g

from gi.repository import Gtk, GLib


class SidebarCover(Hideable):
    def __init__(self, builder):
        self._img_widget = builder.get_object('sidebar_cover_image')
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if (event & Idle.PLAYER) == 0:
            return

        qry = None
        with g.client.lock_currentsong() as song:
            if song is not None:
                qry = configure_query(
                        'cover',
                        artist=song.artist,
                        album=song.album
                )
            else:
                # TODO: Set default image.
                pass

        if qry is not None:
            g.meta_retriever.submit(self._on_item_retrieved, qry)

    def _on_item_retrieved(self, order):
        if order.results:
            cache = order.results[0]
            pixbuf = plyr_cache_to_pixbuf(cache, width=150, height=150)
            if pixbuf is not None:
                self._img_widget.set_from_pixbuf(pixbuf)
