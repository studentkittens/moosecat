#!/usr/bin/env python
# encoding: utf-8

import os
import logging
import logging.handlers

import moosecat.config as cfg
import moosecat.core as core

###########################################################################
#                             Global Registry                             #
###########################################################################


class GlobalRegistry:
    '''
    A Registry for global variables.
    '''
    def register(self, name, ref):
        self.__dict__[name] = ref

    def unregister(self, name):
        del self.__dict__[name]


# This is were the mysterious g-variable comes from.
g = GlobalRegistry()


###########################################################################
#                             PATH CONSTANTS                              #
###########################################################################

def _check_path(path):
    if not os.path.exists(path):
        LOGGER.warn('Path does not exist: ' + path)


def _check_or_mkdir(path):
    if not os.path.exists(path):
        LOGGER.info('Creating directory: ' + path)
        os.mkdir(path)
        _check_path(path)


def _create_xdg_path(envar, endpoint):
    return os.environ.get(envar) or os.path.join(g.HOME_DIR, endpoint)


def _create_file_structure():
    g.register('HOME_DIR', os.path.expanduser('~'))
    g.register('XDG_CONFIG_HOME', _create_xdg_path('XDG_CONFIG_HOME', '.config'))
    g.register('XDG_CACHE_HOME', _create_xdg_path('XDG_CACHE_HOME', '.cache'))

    _check_path(g.HOME_DIR)
    _check_path(g.XDG_CONFIG_HOME)
    _check_path(g.XDG_CACHE_HOME)

    g.register('CONFIG_DIR', os.path.join(g.XDG_CONFIG_HOME, 'moosecat'))
    g.register('CACHE_DIR', os.path.join(g.XDG_CONFIG_HOME, 'moosecat'))

    _check_or_mkdir(g.CONFIG_DIR)
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


def _create_logger(name=None):
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
    logger.setLevel(logging.DEBUG)
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


def boot_moosecat():
    '''Initialize the basic services.

    This is basically a helper to spare us a lot of init work in tests.

    Following things are done:

        - Initialize a pretty logger
        - Load the config
        - Initialize the Plugin System.
        - Load the 'connection' plugin tag
        - Find out to which host/port we should connect. (via Psys)
        - Instance the client and connect to it.
        - If something fails we bailout.
        - return to caller. Caller should load needed plugins then.
    '''
    # prepare filesystem
    _create_file_structure()

    # All logger will inherit from the root logger,
    # so we just customize it.
    _create_logger()

    global LOGGER
    LOGGER = _create_logger('boot')

    logging.debug('A Debug message')
    logging.info('An Info message')
    logging.warning('A Warning message')
    logging.error('An Error message')
    logging.critical('A Critical message')

    config = cfg.Config(filename=g.CONFIG_FILE)

    # Make it known.
    g.register('cfg', config)

    # Logging signals
    client = core.Client()
    g.register('client', client)

    # register auto-logging
    client.signal_add('connectivity', _connectivity_logger)
    client.signal_add('error', _error_logger)
    client.signal_add('progress', _progress_logger)

    # Go into the hot phase...
    client.connect()

    return client.is_connected


def shutdown_moosecat():
    LOGGER.info('Shutting down moosecat')
    g.config.save()
    LOGGER.info('Config saved')
    if client.store is not None:
        client.store.close()
    client.disconnect()


if __name__ == '__main__':
    boot_moosecat()
