#!/usr/bin/env python
# encoding: utf-8

import os
import logging
import logging.handlers

import moosecat.config as cfg
import moosecat.core as core

from moosecat.plugin_system import PluginSystem
from moosecat.heartbeat import Heartbeat
from moosecat.config_defaults import CONFIG_DEFAULTS

# PyXDG Sepcs
from xdg import BaseDirectory


###########################################################################
#                             Global Registry                             #
###########################################################################


class GlobalRegistry:
    '''
    A Registry for global variables.

    Currently you will be able to access:

        * ``client`` - A :class:`moosecat.core.Client` instance.
        * ``psys`` - A loaded Plugin System of :class:`moosecat.plugin_system.PluginSystem`
        * ``config`` - An instance of :class:`moosecat.config`

    '''
    def register(self, name, ref):
        if name not in self.__dict__:
            self.__dict__[name] = ref

    def unregister(self, name):
        if name in self.__dict__:
            del self.__dict__[name]


# This is were the mysterious g-variable comes from.
g = GlobalRegistry()


###########################################################################
#                             PATH CONSTANTS                              #
###########################################################################


def _check_or_mkdir(path):
    if not os.path.exists(path):
        os.mkdir(path)


def _create_xdg_path(envar, endpoint):
    return os.environ.get(envar) or os.path.join(g.HOME_DIR, endpoint)


def _create_file_structure():
    g.register('CONFIG_DIR', os.path.join(BaseDirectory.xdg_config_dirs[0], 'moosecat'))
    _check_or_mkdir(g.CONFIG_DIR)

    g.register('CACHE_DIR', os.path.join(BaseDirectory.xdg_cache_home, 'moosecat'))
    _check_or_mkdir(g.CACHE_DIR)

    g.register('CONFIG_FILE', os.path.join(g.CONFIG_DIR, 'config.yaml'))
    g.register('LOG_FILE', os.path.join(g.CONFIG_DIR, 'app.log'))


###########################################################################
#                                 LOGGING                                 #
###########################################################################

# Get a custom logger for bootup.
LOGGER = None

# Loggin related
COLORED_FORMAT = "%(asctime)s%(reset)s %(log_color)s{logsymbol} \
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
                return result.format(logsymbol=UNICODE_ICONS[record.levelno])

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
        maxBytes=(1024 ** 2 * 10),
        backupCount=2,
        delay=True
    )
    file_stream.setFormatter(formatter)

    # Create the logger and configure it.
    logger.addHandler(file_stream)
    logger.addHandler(stream)
    logger.setLevel(verbosity)
    return logger


def _error_logger(client, error, error_msg, is_fatal):
    log_func = logging.critical if is_fatal else logging.error
    log_func('MPD Error #%d: %s', error, error_msg)


def _progress_logger(client, print_n, message):
    if print_n:
        logging.info('Progress: ' + message)


def _connectivity_logger(client, server_changed):
    if client.is_connected:
        LOGGER.info('connected to %s:%d', client.host, client.port)
    else:
        LOGGER.info('Disconnected from previous server.')
    if server_changed:
        LOGGER.info('Different Server than last time.')


def _find_out_host_and_port():
    host = g.config.get('host')
    if host is None:
        for plugin in g.psys.category('NetworkProvider'):
            data = plugin.find()
            if data is not None:
                return (data[0], data[1])
    else:
        port = g.config.get('port')
        return host, 6600 if port is None else port


def boot_base(verbosity=logging.DEBUG, protocol_machine='command'):
    '''Initialize the basic services.

    This is basically a helper to spare us a lot of init work in tests.

    Following things are done:

        - Prepate the Filesystem.
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
    with open(g.CONFIG_FILE, 'r') as f:
        config.load(f.read())

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
    client.signal_add('error', _error_logger)
    client.signal_add('progress', _progress_logger)

    # Initialize the Plugin System
    psys = PluginSystem()
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

    # Go into the hot phase...
    host, port = _find_out_host_and_port()
    error = client.connect(host, port)
    if error is not None:
        logging.error("Connection Trouble: " + error)

    return client.is_connected


def boot_store(wait=True):
    '''
    Initialize the store (optional)

    Must be called after boot_base()!

    :wait: If True call store.wait() after initialize. If you set wait to False
           you have to call store.wait() yourself, but can do other tasks before.
    :returns: the store (as shortcut) if wait=True, None otherwise.
    '''
    g.client.store_initialize(g.CACHE_DIR)
    if wait:
        g.client.store.wait()
        return g.client.store


def shutdown_application():
    '''
    Destruct everything boot_base() and boot_store() created.

    You can call this function also safely if you did not call boot_store().
    '''
    LOGGER.info('shutting down moosecat.')
    g.config.save(g.CONFIG_FILE)

    try:
        g.client.store.close()
    except AttributeError:
        pass

    g.client.disconnect()
