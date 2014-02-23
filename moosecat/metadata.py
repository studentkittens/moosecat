# stdlib
import os
import time
import collections

from moosecat.core import MetadataThreads
from moosecat.boot import g

# external
from gi.repository import GLib
import plyr


Order = collections.namedtuple('Order', [
    'notify_func',
    'cache_received_func',
    'query',
    'timestamp',
    'results'
])


def make_query_key(query):
    """Returns a hash value that is unique to the configuration of a plry.Query
    """
    return hash((
        query.artist, query.album, query.title, query.get_type, query.number,
        query.normalize, query.useragent, tuple(query.providers)
    ))


class Retriever(MetadataThreads):
    """Multithreaded (on the C-side) retrieval of metadata queries.
    """
    def __init__(self):
        self._query_dict = {}
        self._database = plyr.Database(g.CACHE_DIR)
        MetadataThreads.__init__(
            self,
            self._on_thread,
            self._on_deliver
        )

    def push(self, query, notify_func, cache_received_func=None):
        """Push a new query into the Retriever.

        Internally the query will be wraped into an Order.
        The Order provides a results (list of plyr.Caches) and a query field,
        also there's the time of the Order initialization in timestamp.

        If the query (or one very similar) is currently processed push() will
        return immediately.

        Callbacks can be provided to get the results of the query, both will be
        called on the main thread, for a stress-free threading experience:

            * ``notify_func``: called with the order as soon commit() finished
            * ``cache_received_func``: called, possible several times, as soon a
               single cache is ready. The order and the cache is passed.
               The cache will not be in the results field of the order.
               Do not store the cache, it is temporarily and will be destroyed in
               the background. This is only meant for "render or surrender".

        :returns: the timestamp of the point where the order was created.
        """
        if not any((notify_func, query)):
            return None

        key = make_query_key(query)
        if key in self._query_dict:
            return None

        # The order wraps all the extra data:
        order = Order(
            notify_func=notify_func,
            cache_received_func=cache_received_func,
            query=query,
            timestamp=time.time(),
            results=[]
        )

        # Configure to use the database if possible.
        query.database = self._database
        if cache_received_func:
            query.callback = self._on_query_callback

        # Remember the order.
        self._query_dict[key] = order

        # Distribute it to some thread on the C-side
        MetadataThreads.push(self, order)

        # Users might use this to know if the need to update.
        return order.timestamp

    def _on_query_callback(self, cache, query):
        """Called by libglyr's wrapper code as soon a cache is ready.
        cache and query wrap a valid C-struct but they are *not* the same
        object as in results or the query above.

        Attention: does not run on mainthread!
        """
        self.forward((cache, query))

    def _on_thread(self, mdt, order):
        """Called in an own thread, as soon a thread is ready.

        Attention: does not run on mainthread!
        """
        # Trick: commit() does not lock the gil most of the time.
        results = order.query.commit()
        order.results.extend(results)
        return order

    def _on_deliver(self, mdt, order_or_cache):
        """Called as soon an item is ready on the mainthread.
        """
        if isinstance(order_or_cache, Order):
            order = order_or_cache
            order.notify_func(order)
            self._query_dict.pop(make_query_key(order.query))
        else:
            # Not finished yet, subresult:
            cache, query = order_or_cache

            # Get the corresponding order:
            order = self._query_dict[make_query_key(query)]

            # Call the callback if possible:
            order.cache_received_func(order, cache)

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
        for order in self._query_dict.values():
            order.query.cancel()


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
