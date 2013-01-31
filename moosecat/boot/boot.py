#!/usr/bin/env python
# encoding: utf-8

import logging
import logging.handlers

# Filesystem Paths
import moosecat.boot.fs as fs
import moosecat.config as cfg
import moosecat.core as core


# Global Variables that are used across the whole program:
client = core.Client()
config = None

# Get a custom logger for bootup.
_LOGGER = None

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


def create_logger(name=None):
    '''Create a new Logger configured with moosecat's defaults.

    :name: A user-define name that describes the logger
    :return: A new logger .
    '''
    logger = logging.getLogger(name)

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
        filename=fs.LOG_FILE,
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


def error_logger(client, error, error_msg, is_fatal):
    log_func = logging.critical if is_fatal else logging.error
    log_func('MPD Error #%d: %s', error, error_msg)


def progress_logger(client, print_n, message):
    if print_n:
        logging.info('Progress: ' + message)


def connectivity_logger(client, server_changed):
    if client.is_connected:
        _LOGGER.info('connected to %s:%d', client.host, client.port)
    else:
        _LOGGER.info('Disconnected from previous server.')
    if server_changed:
        _LOGGER.info('Different Server than last time.')


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

    # All logger will inherit from the root logger,
    # so we just customize it.
    create_logger()

    global _LOGGER
    _LOGGER = create_logger('boot')

    #logging.debug('A Debug message')
    #logging.info('An Info message')
    #logging.warning('A Warning message')
    #logging.error('An Error message')
    #logging.critical('A Critical message')

    global config
    config = cfg.Config(filename=fs.CONFIG_FILE)

    # Logging signals
    client.signal_add('connectivity', connectivity_logger)
    client.signal_add('error', error_logger)
    client.signal_add('progress', progress_logger)

    client.connect()
    if client.is_connected:
        _LOGGER.info('Connected')


def shutdown_moosecat():
    _LOGGER.info('Shutting down moosecat')
    config.save()
    _LOGGER.info('Config saved')
    if client.store is not None:
        client.store.close()
    client.disconnect()


if __name__ == '__main__':
    boot_moosecat()
