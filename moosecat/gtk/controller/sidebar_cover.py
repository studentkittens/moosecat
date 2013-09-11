from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.utils import plyr_cache_to_pixbuf
from moosecat.metadata import configure_query_by_current_song
from moosecat.core import Idle, Status
from moosecat.boot import g


class SidebarCover(Hideable):
    def __init__(self, builder):
        self._img_widget = builder.get_object('sidebar_cover_image')
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & Idle.PLAYER == 0:
            return

        g.meta_retriever.submit(
            self._on_item_retrieved,
            configure_query_by_current_song('cover')
        )

    def _on_item_retrieved(self, order):
        if len(order.results) > 0:
            cache, *_ = order.results
            pixbuf = plyr_cache_to_pixbuf(cache, width=150, height=150)
            if pixbuf is not None:
                self._img_widget.set_from_pixbuf(pixbuf)
