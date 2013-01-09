#!/usr/bin/env python
# encoding: utf-8

import logging
import logging.handlers


# Filesystem Paths
import moosecat.boot.fs as fs
import moosecat.config as config
import moosecat.core as core


COLORED_FORMAT = "%(asctime)s%(reset)s %(log_color)s{logsymbol}  %(levelname)-8s%(reset)s %(bold_blue)s[%(filename)s:%(lineno)3d] %(bold_black)s%(name)s: %(reset)s%(message)s"
SIMPLE_FORMAT = "%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)3d] %(name)s: %(message)s"
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
    logger = logging.getLogger(name)
    logger.addHandler(file_stream)
    logger.addHandler(stream)
    logger.setLevel(logging.DEBUG)
    return logger


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

    # Get a custom logger for bootup.
    logger = logging.getLogger('boot')

    logger.debug('a debug message')
    logger.info('an info message')
    logger.warn('a warning message')
    logger.error('an error message')
    logger.critical('a critical message')

    cfg = config.Config(filename=fs.CONFIG_FILE)
    cfg.load()
    cfg.set('lala', [1, 22, 3])
    cfg.save()

    client = core.Client()
    err = client.connect()
    if client.is_connected:
        logger.info('Connected')
    else:
        logger.warn('Not connected: ' + err)

    client.disconnect()
    logger.info('Disconnect')

if __name__ == '__main__':
    boot_moosecat()
