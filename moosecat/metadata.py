# stdlib
import os
import logging
import queue
import threading
import time
import collections

from moosecat.core import MetadataThreads
from moosecat.boot import g

# external
from gi.repository import GLib
import plyr


Order = collections.namedtuple('Order', [
    'notify_func',
    'query',
    'timestamp',
    'results'
])


class Retriever(MetadataThreads):
    def __init__(self):
        self._query_set = set()
        self._database = plyr.Database(g.CACHE_DIR)
        MetadataThreads.__init__(
            self,
            self._on_thread,
            self._on_deliver
        )

    def push(self, notify_func, query):
        if all((notify_func, query)):
            # The order wraps all the extra data:
            order = Order(
                notify_func=notify_func,
                query=query,
                timestamp=time.time(),
                results=[]
            )

            # Configure to use the database if possible.
            query.database = self._database

            # Remember the order.
            self._query_set.add(query)

            # Distribute it to some thread on the C-side
            MetadataThreads.push(self, order)

            return order.timestamp

    def _on_thread(self, mdt, order):
        # Trick: commit() does not lock the gil most of the time.
        results = order.query.commit()
        order.results.extend(results)
        return order

    def _on_deliver(self, mdt, order):
        order.notify_func(order)
        self._query_set.remove(order.query)

    def lookup(self, query):
        '''
        Lookup an item in the Database

        :query: plyr.Query, as returned from configure_query e.g.
        :returns: Boolean.
        '''
        return self._database.lookup(query)

    def close(self):
        '''
        Close the metadata system (join threads).

        .. note::

            This might be already called for you automatically if you used boot_metadata()

        .. note::

            This should be called on the main thead.
        '''
        for query in self._query_set:
            query.cancel()


def update_needed(last_query, new_query):
    '''
    Check if an update of the metadata is actually needed.
    It does this by checking the required attributes and compares them.

    :last_query: Query used on the last update
    :new_query: New Query that will be used.
    :returns: Boolean
    '''
    if not all((last_query, new_query)):
        return True

    if not last_query.get_type == new_query.get_type:
        return True

    info = plyr.PROVIDERS.get(last_query.get_type)
    for attribute in info['required']:
        if not getattr(last_query, attribute) == getattr(new_query, attribute):
            return True
    return False


def _get_full_current_song_uri():
    base_path = g.server_profile.music_directory
    if base_path is None:
        return

    with g.client.lock_currentsong() as song:
        if song is not None and dir_path is not None:
            full_path = os.path.join(base_path, song.uri)
        else:
            full_path = None

    if full_path is not None and os.access(full_path, os.R_OK):
        return full_path


def configure_query_by_current_song(get_type, amount=1, img_size=(-1, -1)):
    '''
    Configure a Query based on the current song.
    If no current song currently None is returned.

    Otherwise the same as :py:func:`configure_query`

    :returns: a :py:class:`plyr.Query`
    '''
    with g.client.lock_currentsong() as song:
        if song is not None:
            qry = configure_query(
                get_type,
                song.artist,
                song.album,
                song.title,
                amount=1,
                img_size=img_size
            )
        else:
            qry = None

    return qry


def configure_query(get_type, artist=None, album=None, title=None, amount=1, img_size=(-1, -1)):
    '''
    Create a :class:`plyr.Query` based on the parameters.
    Not all parameters are required, for cover only artist/album is needed.

    To get a full list of get_types do: ::

        >>> from plyr import PROVIDERS
        >>> list(PROVIDERS.keys())

    Other values will be read from the config or determined from runtime info or
    the values are left to libglyr to be determined.

    :param get_type: A get_type dictated by plyr ('cover', 'lyrics')
    :param artist: Artist Information
    :param album: Album Information
    :param title: Title information
    :param amount: Number of items to retrieve
    :param img_size: If item is an image, the (min size, max size) as tuple.
    :returns: a ready configured :class:`plyr.Query`
    '''
    def bailout(msg):
        raise ValueError(': '.join([msg, str(get_type)]))

    if artist is None:
        bailout('Artist is required for this get_type')

    info = plyr.PROVIDERS.get(get_type)
    if info is None:
        bailout('Not a valid get_type')

    requirements = info['required']
    if 'title' in requirements and title is None:
        bailout('Title is required for this get_type')

    if 'album' in requirements and album is None:
        bailout('Album is required for this get_type')

    # Write less, one indirection less
    cfg = g.config

    return plyr.Query(
        **{
            key: value for key, value in dict(
                # Settings explicitly set
                get_type=get_type,
                artist=str(artist),
                album=str(album),
                title=str(title),
                number=amount,
                musictree_path=_get_full_current_song_uri(),
                force_utf8=True,
                useragent='moosecat/0.0.1 +(https://github.com/studentkittens/moosecat)',
                img_size=img_size,

                # Settings dynamically calculated by libglyr
                # allowed_formats - png, jpg, gif
                # proxy           - Read from env vars
                # max_per_plugin  - Determined per get_type, number etc.
                # parallel        - Determined per get_type
                # language        - Autodetected from locale

                # Configurable Attributes
                verbosity=cfg['metadata.verbosity'],
                providers=cfg['metadata.providers'],
                language_aware_only=cfg['metadata.language_aware_only'],
                qsratio=cfg['metadata.quality_speed_ratio'],
                redirects=cfg['metadata.redirects'],
                timeout=cfg['metadata.timeout'],
                fuzzyness=cfg['metadata.fuzzyness']
            ).items()
            if value is not None
        }
    )


if __name__ == '__main__':
    try:
        def on_item_received(order):
            for result in order.results:
                result.print_cache()

        ret = Retriever()
        ret.submit(on_item_received, configure_query('lyrics', 'Akrea', title='Trugbild'))

        loop = GLib.MainLoop()
        GLib.timeout_add(10000, loop.quit)
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        ret.close()
