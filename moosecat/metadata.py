# stdlib
import logging
import queue
import threading

from moosecat.boot import g

# external
from gi.repository import GLib
import plyr


class Order:
    def __init__(self, notify, query):
        self._notify, self._query = notify, query
        self._results = []

    def __lt__(self, other):
        # PriorityQueue requires that both elements of the tuple
        # are sortable, so that items with the same priority can be
        # priorized - in our case we ignore this and treat all orders the same.
        return 0

    @property
    def results(self):
        return self._results

    def execute(self):
        'Execute the Query synchronously'
        self._results = self._query.commit()

    def call_notify(self):
        'Call the user-defined callback for this order'
        if self._notify:
            self._notify(self)


class Retriever:
    def __init__(self, threads=4):
        self._database = plyr.Database('/tmp')  # TODO
        self._order_queue = queue.PriorityQueue()
        self._fetch_queue = queue.Queue()
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
        :returns: an :class:`moosecat.metadata.Order` object.
        '''
        if all((notify, query)):
            query.database = self._database
            order = Order(notify, query)
            self._order_queue.put((prio, order))
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

        '''
        GLib.source_remove(self._watch_source)
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


def _get_current_song_uri():
    # TODO: Implement profiles first
    dir_path = g.server_profile.music_directory
    with g.client.lock_currentsong() as song:
        if song is not None and dir_path is not None:
            full_path = os.path.join(dir_path, song.uri)
            if os.access(full_path, os.R_OK):
                return full_path


def configure_query(get_type, artist=None, album=None, title=None, amount=1):
    # TODO: this would be the best place to read the config.
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

    return plyr.Query(
        get_type=get_type,
        artist=str(artist),
        album=str(album),
        title=str(title),
        number=amount,
        verbosity=2,
        musictree_path=_get_current_song_uri() or ''
        ############################################
        #  Config Values, not explicitly settable  #
        ############################################
        # not settable;
        # allowed_formats,
        # proxy
        # useragent
        # max_per_plugin
        # parallel

        # # automatically:
        # database_path
        # force_utf8
        # img_size,
        # language

        # fuzzyness
        # language_aware_only
        # providers
        # qsratio
        # redirects
        # timeout
        # verbosity
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
