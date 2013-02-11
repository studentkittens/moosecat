'''moocat.

moocat is the commandline interface to moosecat, a versatile MPD Client.

Usage:
    moocat
    moocat <command> [options] [-v...]
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
    -q --quiet           Try to print no output. (Except very unexpected errors)

Misc Options:
    -H --help            Show this help text.
    -V --version         Show a summary about moocat's version and the libraries behind.

Examples:

    moocat list -h localhost -p 6600 -vvv
'''

import sys
import logging
import moosecat.boot.boot as boot


try:
    import docopt
except ImportError:
    print('-- docopt not found. Please run:')
    print('-- pip install docopt           ')
    sys.exit(-1)


if __name__ == '__main__':
    args = docopt.docopt(__doc__, version='moocat 0.1')
    logger = logging.getLogger('moocat')

    boot.boot_moosecat()

    command = args['<command>']
    if command == 'next':
        boot.client.player_next()
    elif command == 'prev':
        boot.client.player_previous()
    elif command == 'status':
        print(boot.client.status)
    elif command == 'outputs':
        for output in boot.client.outputs:
            print('#{oid}\t{name}\t{on}'.format(oid=output.oid, name=output.name, on=output.enabled))
    elif command == 'view':
        boot.client.store_initialize('/tmp')
        store = boot.client.store
        store.wait_mode = True
        store.wait()

        for name in store.stored_playlist_names:
            store.stored_playlist_load(name)

        for song in store.stored_playlist_search('test', 'e*'):
            print(song.title)

    boot.shutdown_moosecat()
