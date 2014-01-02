#!/usr/bin/env python
# encoding: utf-8


###########################################################################
#                             Global Registry                             #
###########################################################################


class GlobalRegistry:
    '''
    A Registry for global variables.

    Currently you will be able to access:

        * ``client``    - A :class:`moosecat.core.Client` instance.
        * ``psys``      - A loaded Plugin System of :class:`moosecat.plugin_system.PluginSystem`
        * ``config``    - An instance of :class:`moosecat.config`
        * ``heartbeat`` - An instance of :class:`moosecat.Heartbeat`

    '''
    def register(self, name, ref):
        if not hasattr(self, name):
            setattr(self, name, ref)

    def unregister(self, name):
        if not hasattr(self, name):
            delattr(self, name)


# This is were the mysterious g-variable comes from.
g = GlobalRegistry()

###########################################################################
#                                 Imports                                 #
###########################################################################

# Note: Imports come after GlobalRegistry since some of them
# may do 'from moosecat.boot import g' already.

import os
import logging
import logging.handlers

import moosecat.config as cfg
import moosecat.core as core
import moosecat.metadata as metadata

from moosecat.plugin_system import PluginSystem
from moosecat.heartbeat import Heartbeat
from moosecat.config_defaults import CONFIG_DEFAULTS
from moosecat.server_profile import ServerProfile

# PyXDG Sepcs
from xdg import BaseDirectory

###########################################################################
#                             PATH CONSTANTS                              #
###########################################################################


def _create_xdg_path(envar, endpoint):
    return os.environ.get(envar) or os.path.join(g.HOME_DIR, endpoint)


def _create_file_structure():
    def check_or_mkdir(path):
        if not os.path.exists(path):
            os.mkdir(path)

    g.register('CONFIG_DIR', os.path.join(BaseDirectory.xdg_config_dirs[0], 'moosecat'))
    check_or_mkdir(g.CONFIG_DIR)

    g.register('CACHE_DIR', os.path.join(BaseDirectory.xdg_cache_home, 'moosecat'))
    check_or_mkdir(g.CACHE_DIR)

    g.register('USER_PLUGIN_DIR', os.path.join(g.CONFIG_DIR, 'plugins'))
    check_or_mkdir(g.USER_PLUGIN_DIR)

    g.register('CONFIG_FILE', os.path.join(g.CONFIG_DIR, 'config.yaml'))
    g.register('LOG_FILE', os.path.join(g.CONFIG_DIR, 'app.log'))


###########################################################################
#                                 LOGGING                                 #
###########################################################################

# Get a custom logger for bootup.
LOGGER = None

# Loggin related
COLORED_FORMAT = "%(asctime)s%(reset)s %(log_color)s[logsymbol] \
%(levelname)-8s%(reset)s %(bold_blue)s[%(filename)s:%(lineno)3d]%(reset)s \
%(bold_black)s%(name)s:%(reset)s %(message)s"

SIMPLE_FORMAT = "%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)3d] \
%(name)s: %(message)s"

DATE_FORMAT = "%H:%M:%S"
UNICODE_ICONS = {
    logging.DEBUG: '⚙',
    logging.INFO: '⚐',
    logging.WARNING: '⚠',
    logging.ERROR: '⚡',
    logging.CRITICAL: '☠'
}


def _create_logger(name=None, verbosity=logging.DEBUG):
    '''Create a new Logger configured with moosecat's defaults.

    :name: A user-define name that describes the logger
    :return: A new logger .
    '''
    logger = logging.getLogger(name)

    # This is hack to see if this function was already called
    if len(logging.getLogger(None).handlers) is 2:
        return logger

    # Defaultformatter, used for File logging,
    # and for stdout if colorlog is not available
    formatter = logging.Formatter(
        SIMPLE_FORMAT,
        datefmt=DATE_FORMAT
    )

    # Try to load the colored log and use it on success.
    # Else we'll use the SIMPLE_FORMAT
    try:
        import colorlog

        class SymbolFormatter(colorlog.ColoredFormatter):
            def format(self, record):
                result = colorlog.ColoredFormatter.format(self, record)
                return result.replace('[logsymbol]', UNICODE_ICONS[record.levelno])

    except ImportError:
        col_formatter = formatter
    else:
        col_formatter = SymbolFormatter(
            COLORED_FORMAT,
            datefmt=DATE_FORMAT,
            reset=False
        )

    # Stdout-Handler
    stream = logging.StreamHandler()
    stream.setFormatter(col_formatter)

    # Rotating File-Handler
    file_stream = logging.handlers.RotatingFileHandler(
        filename=g.LOG_FILE,
        maxBytes=(1024 ** 2 * 10),  # 10 MB
        backupCount=2,
        delay=True
    )
    file_stream.setFormatter(formatter)

    # Create the logger and configure it.
    logger.addHandler(file_stream)
    logger.addHandler(stream)
    logger.setLevel(verbosity)
    return logger


def _logging_logger(client, message, level):
    logfunc = {
        'critical': logging.critical,
        'error': logging.error,
        'warning': logging.warning,
        'info': logging.info,
        'debug': logging.debug
    }.get(level, logging.info)
    logfunc('Logging: ' + message)


def _connectivity_logger(client, server_changed, was_connected):
    if client.is_connected:
        LOGGER.info('connected to %s:%d', client.host, client.port)
    else:
        LOGGER.info('Disconnected from previous server.')
    if server_changed:
        LOGGER.info('Different Server than last time.')


def _find_out_host_and_port():
    host = g.server_profile.host or 'localhost'
    port = g.server_profile.port or 6600
    return host, port
    #if host is None:
    #    for plugin in g.psys.category('NetworkProvider'):
    #        data = plugin.find()
    #        if data is not None:
    #            return (data[0], data[1])  # (host, port)
    #else:
    #    port = g.config.get('port')
    #    return host, 6600 if port is None else port


def boot_base(verbosity=logging.INFO, protocol_machine='idle'):
    '''Initialize the basic services.

    This is basically a helper to spare us a lot of init work in tests.

    Following things are done:

        - Prepare the Filesystem.
        - Initialize a pretty root logger.
        - Load the config.
        - Initialize the Plugin System.
        - Instance the client and connect to it.
        - Find out to which host/port we should connect (config and NetworProvider).
        - Make everything available under the 'g' variable;

    :protocol_machine: The pm to use for the Client. Please know what you do.
    :verbosity: Verbosity to use during bootup. You can adjust this using ``logger.getLogger(None).setLevel(logger.YourLevel)`` later.
    :returns: True if erverything worked and the client is connected.
    '''
    # prepare filesystem
    _create_file_structure()

    # All logger will inherit from the root logger,
    # so we just customize it.
    _create_logger(verbosity=verbosity)

    global LOGGER
    LOGGER = _create_logger('boot', verbosity=verbosity)

    # Set up Config
    config = cfg.Config()
    try:
        with open(g.CONFIG_FILE, 'r') as f:
            config.load(f.read())
    except FileNotFoundError:
        pass

    config.add_defaults(CONFIG_DEFAULTS)
    config.add_defaults({
        'paths': {
            'config_home': g.CONFIG_DIR,
            'cache_home': g.CACHE_DIR
        }
    })

    # Make it known.
    g.register('config', config)

    # Logging signals
    client = core.Client(protocol_machine=protocol_machine)
    g.register('client', client)

    # Redirect GLib Errors to the error signal (needs to know what client's signals)
    core.register_external_logs(client)

    # register auto-logging
    client.signal_add('connectivity', _connectivity_logger)
    client.signal_add('logging', _logging_logger)

    # Initialize the Plugin System
    psys = PluginSystem(config=g.config, extra_plugin_paths=[g.USER_PLUGIN_DIR])
    g.register('psys', psys)

    # Acquire Config Defaults for all Plugins
    for plugin in psys.list_plugin_info_by_category('ConfigDefaults'):
        name = plugin.name
        config.add_defaults({
            'plugins': {
                name: plugin.plugin_object.get_config_defaults()
            }
        })

    # do the Heartbeat stuff
    g.register('heartbeat', Heartbeat(client))

    # Make use of the Server Profiles in the config
    # Note: This needs the Pluginsystem
    server_profile = ServerProfile(config)
    g.register('server_profile', server_profile)
    server_profile.trigger_scan(psys)

    # Go into the hot phase...
    host, port = _find_out_host_and_port()
    error = client.connect(host, port)
    if error is not None:
        logging.error("Connection Trouble: " + error)

    return client.is_connected


def boot_metadata():
    '''
    Initialize the metadata system (optional)

    After calling this function you can use the :class:`moosecat.metadata.Retriever` instance
    in ``g.meta_retriever``.
    '''
    retriever = metadata.Retriever()
    g.register('meta_retriever', retriever)


def boot_store():
    '''
    Initialize the store (optional)

    Must be called after boot_base()!

    :returns: the store (as shortcut) if wait=True, None otherwise.
    '''
    g.client.store_initialize(g.CACHE_DIR)
    g.register('store', g.client.store)
    logging.info('Started store creation in background')
    return g.client.store


def _format_db_path():
    return os.path.join(g.CACHE_DIR, 'moosecat_{host}:{port}.sqlite[.zip]'.format(
        host=g.client.host, port=g.client.port
    ))


def shutdown_application():
    '''
    Destruct everything boot_base() and boot_store() created.

    You can call this function also safely if you did not call boot_store() or boot_metadata().
    '''
    LOGGER.info('Shutting down moosecat.')
    g.config.save(g.CONFIG_FILE)

    try:
        g.client.store.close()
        logging.info('Saving database to: ' + _format_db_path())
    except AttributeError:
        pass

    g.client.disconnect()

    if hasattr(g, 'meta_retriever'):
        g.meta_retriever.close()


if __name__ == '__main__':
    boot_base()
    boot_store()

    with g.store.query('Akrea') as songs:
        for song in songs:
            print(song.artist, song.album, song.title)

    shutdown_application()
