from moosecat.gtk.interfaces import Hideable
from moosecat.gtk.utils import plyr_cache_to_pixbuf
from moosecat.metadata import configure_query_by_current_song, update_needed
from moosecat.core import Idle, Status
from moosecat.boot import g

import logging
LOGGER = logging.getLogger('GtkSidebarCover')


class SidebarCover(Hideable):
    def __init__(self, builder):
        self._img_widget = builder.get_object('sidebar_cover_image')
        g.client.signal_add('client-event', self._on_client_event)
        self._last_query = None

    def _on_client_event(self, client, event):
        if not event & Idle.PLAYER:
            return

        qry = configure_query_by_current_song('cover')
        if not update_needed(self._last_query, qry):
            LOGGER.debug('sidebar cover needs no update')
            return

        self._submit_tmstmp = g.meta_retriever.push(
            qry,
            self._on_item_retrieved
        )

    def _on_item_retrieved(self, order):
        if order.results and self._submit_tmstmp <= order.timestamp:
            cache, *_ = order.results
            pixbuf = plyr_cache_to_pixbuf(cache, width=150, height=150)
            if pixbuf is not None:
                self._img_widget.set_from_pixbuf(pixbuf)
                self._last_query = order.query
