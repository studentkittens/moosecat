# stdlib
import logging
import queue
import threading

from moosecat.boot import g

# external
from gi.repository import GLib
import plyr


class Order:
    '''Create a new Order used to deliver results.

    You usually don't need to create new order explicitly, since they're
    created by the :class:`Retriever` class for you.
    '''
    def __init__(self, notify, query):
        '''
        :param notify: A callable, called when query is done cooking.
        :param query: a plyr.Query
        '''
        self._notify, self._query = notify, query
        self._results = None

    def __lt__(self, other):
        # PriorityQueue requires that both elements of the tuple
        # are sortable, so that items with the same priority can be
        # priorized - in our case we ignore this and treat all orders the same.
        return 0

    @property
    def results(self):
        'A list of plyr.Cache or None if nothing happened yet.'
        return self._results

    def execute(self):
        'Execute the Query synchronously'
        self._results = self._query.commit()

    def call_notify(self):
        'Call the user-defined callback for this order'
        if self._notify:
            self._notify(self)


class Retriever:
    '''Models a metadata retrieveal system.

    Queries can be submitted to the retriever which distributes those to a number
    of threads. The notify-callable passed alongside the Query will be called
    ON THE MAINTHREAD when the Query is done processing.
    '''
    def __init__(self, threads=4):
        '''Create a new Retriever with N threads.'''
        self._database = plyr.Database(g.CACHE_DIR)
        self._order_queue = queue.PriorityQueue()
        self._fetch_queue = queue.Queue()
        self._query_set = set()
        self._thr_barrier = threading.Barrier(
                parties=threads + 1,
                timeout=1.0  # Time to wait before forced kill.
        )

        self._fetch_threads = []
        for num in range(threads):
            thread = threading.Thread(
                    target=self._threaded_fetcher,
                    name='MetadataFetcherThread #' + str(num)
            )
            thread.start()
            self._fetch_threads.append(thread)
        self._watch_source = GLib.timeout_add(200, self._watch_fetch_queue)

    def _wait_on_barrier(self):
        try:
            self._thr_barrier.wait()
        except threading.BrokenBarrierError:
            pass

    def _threaded_fetcher(self):
        while True:
            _, order = self._order_queue.get()
            self._order_queue.task_done()
            if order is not None:
                try:
                    order.execute()
                    self._fetch_queue.put(order)
                except:
                    logging.exception('Unhandled exception while retrieving metadata')
            else:
                self._wait_on_barrier()
                break

    def _watch_fetch_queue(self):
        try:
            order = self._fetch_queue.get_nowait()
            order.call_notify()
            self._query_set.remove(order._query)
        except queue.Empty:
            pass
        except:
            logging.exception('Exception during executing order.notfiy()')
        finally:
            return True

    ####################
    #  Public Methods  #
    ####################

    def submit(self, notify, query, prio=0):
        '''
        Submit a request to the metadata System.

        :param notify: a callable that is called on main thread when the item is retrieved
        :param query: a plyr.Query, like for example from ``configure_query()``
        :param prio: Higher priorites get sorted earlier in the Job Queue
        :returns: an unfinished :class:`moosecat.metadata.Order` object.
        '''
        if all((notify, query)):
            query.database = self._database
            order = Order(notify, query)
            self._order_queue.put((prio, order))
            self._query_set.add(query)
            return order

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
        GLib.source_remove(self._watch_source)

        # Tell left-over queries to cancel
        for query in self._query_set:
            query.cancel()

        # Tell worker threads to finish
        for thread in self._fetch_threads:
            self._order_queue.put((-1, None))


        self._wait_on_barrier()


def update_needed(last_query, new_query):
    '''
    Check if an update of the metadata is actually needed.
    It does this by checking the required attributes and compares them.

    :last_query: Query used on the last update
    :new_query: New Query that will be used.
    :returns: Boolean
    '''
    if not last_query.get_type == new_query.get_type:
        return False

    info = plyr.PROVIDERS.get(last_query.get_type)
    for attribute in info['required']:
        if not getattr(last_query, attribute) == getattr(new_query, attribute):
            return False
    return True


def _get_full_current_song_uri():
    base_path = g.server_profile.music_directory
    if base_path is None:
        return

    with g.client.lock_currentsong() as song:
        if song is not None and dir_path is not None:
            full_path = os.path.join(base_path, song.uri)
            if os.access(full_path, os.R_OK):
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
            return configure_query(
                    get_type,
                    song.artist,
                    song.album,
                    song.title,
                    amount=1,
                    img_size=img_size
            )


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
