'''moocat.

moocat is the commandline interface to moosecat, a versatile MPD Client.

Usage:
    moocat
    moocat <command> [options] [-v...] [-q...]
    moocat -H | --help
    moocat -V | --version

Options:
    -h --host=<addr>     Define the server to connect to [default: localhost]
    -p --port=<port>     Define the server's port. [default: 6600]
    -P --password=<pwd>  Authenticate with this password on the server.
    -d --daemonize       Fork to background and run till closed.
    -c --self-connect    Connect to a daemonized instance, instead of building db itself.
    -w --wait            Wait for operations to finish (database updates, idle) [default: no]
    -v --verbose         Print more output than usual. Might be given more than once to increase the verbosity.
    -q --quiet           Try to print no output. (Except very unexpected errors), can be passed more than once.

Misc Options:
    -H --help            Show this help text.
    -V --version         Show a summary about moocat's version and the libraries behind.

Examples:

    moocat list -h localhost -p 6600 -vvv
'''

import sys
import logging
from moosecat.boot import g, shutdown_application, boot_base, boot_store



try:
    import docopt
except ImportError:
    print('-- docopt not found. Please run:')
    print('-- pip install docopt           ')
    sys.exit(-1)


if __name__ == '__main__':
    args = docopt.docopt(__doc__, version='moocat 0.1')
    logger = logging.getLogger('moocat')

    verbosity = 2 + args['--verbose'] - args['--quiet']
    boot_base(verbosity=verbosity_to_loglevel(verbosity))

    command = args['<command>']
    if command == 'next':
        g.client.player_next()
    elif command == 'prev':
        g.client.player_previous()
    elif command == 'status':
        print(g.client.status)
    elif command == 'outputs':
        for output in g.client.outputs:
            print('#{oid}\t{name}\t{on}'.format(oid=output.oid, name=output.name, on=output.enabled))
    elif command == 'view':
        g.client.store_initialize('/tmp')
        store = g.client.store
        store.wait_mode = True
        store.wait()

        for name in store.stored_playlist_names:
            store.stored_playlist_load(name)

        for song in store.stored_playlist_search('test', 'e*'):
            print(song.title)
    elif command == 'queue':
        g.client.store_initialize('/tmp')
        store = g.client.store
        store.wait()

        for song in store.query():
            print('{a:25} {b:>45} {t:>40}'.format(
                a=song.artist,
                b=song.album,
                t=song.title
            ))

    elif command == 'list-plugins':
        categories = g.psys.list_categories()
        for category in categories:
            print(category)
            for info in g.psys.list_plugin_info_by_category(category):
                print('  name       : ', info.name)
                print('  description: ', info.description)
                print('  version    : ', info.version)

    elif command == 'guess-host':
        for plugin in g.psys.category('NetworkProvider'):
            result = plugin.find()
            if result is not None:
                host, port = result
                print(host, ':', port)
                break
        else:
            print('Nothing found. Not even a default.')

    elif command == 'dir':
        store = boot_store()

        def print_dirs(path=None, depth=-1):
            for p in store.query_directories(path, depth):
                print(p)

            print('-' * 40)

        print_dirs(None, 0)
        print_dirs(None, 1)
        print_dirs('Musik/Knorkator/Das nächste Album aller Zeiten', -1)
        print_dirs('*.flac', -1)

    shutdown_application()
