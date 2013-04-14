'''Moosecat is a versatile MPD Client.

Usage:
    moocat
    moocat [options] [-v...] [-q...]
    moocat -H | --help
    moocat -V | --version

Options:
    -h --host=<addr>     Define the server to connect to [default: localhost]
    -p --port=<port>     Define the server's port. [default: 6600]
    -P --password=<pwd>  Authenticate with this password on the server. [default:]
    -t --timeout=<sec>   Wait sec seconds before a timeout occures. [default: 2.0]
    -w --wait            Wait for operations to finish (database updates, idle) [default: no]
    -v --verbose         Print more output than usual. Might be given more than once to increase the verbosity.
    -q --quiet           Try to print no output. (Except very unexpected errors), can be passed more than once.

Misc Options:
    -H --help            Show this help text.
    -V --version         Show a summary about moocat's version and the libraries behind.

Examples:

    moocat list -h localhost -p 6600 -vvv
'''

import logging

try:
    import docopt
except ImportError:
    print('-- docopt not found. Please run:')
    print('-- pip install docopt           ')
    sys.exit(-1)


def parse_arguments(cfg):
    args = docopt.docopt(__doc__, version='0.1')

    def path(endpoint):
        return '.'.join(['profiles', cfg['active_profile'], endpoint])

    cfg[path('host')] = args['--host']
    cfg[path('port')] = args['--port']
    cfg[path('timeout')] = args['--timeout']
    cfg['verbosity'] = args['--verbose']

    password = args['--password']
    if password != '':
        cfg[path('password')] = password

    print(args)
    print(cfg)


if __name__ == '__main__':
    from moosecat.config import Config
    from moosecat.config_defaults import CONFIG_DEFAULTS
    cfg = Config()
    cfg.add_defaults(CONFIG_DEFAULTS)
    parse_arguments(cfg)
