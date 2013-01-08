#!/usr/bin/env python
# encoding: utf-8

import logging


# Filesystem Paths
import moosecat.boot.fs as fs
import moosecat.config as config


def create_logger(name):
    logger = logging.getLogger(name)
    stream = logging.StreamHandler()

    try:
        import colorlog
        formatter = colorlog.ColoredFormatter(
            "%(asctime)s%(reset)s %(log_color)s%(levelname)-8s%(reset)s %(bold_blue)s[%(filename)s:%(lineno)d] %(bold_black)s%(name)s: %(reset)s%(message)s",
            datefmt="%H:%M:%S"
        )

        stream.setFormatter(formatter)
    except ImportError:
        pass

    # TODO...
    file_stream = logging.FileHandler(fs.LOG_FILE)
    file_stream.setLevel(logging.DEBUG)
    file_stream.setFormatter(formatter)
    logger.addHandler(file_stream)

    logger.addHandler(stream)
    logger.setLevel(logging.DEBUG)
    return logger


def boot_moosecat():
    '''
    Initialize the basic services.

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
    create_logger(None)

    # Get a custom logger for bootup.
    logger = logging.getLogger('moosecatboot')

    logger.debug('a debug message')
    logger.info('an info message')
    logger.warn('a warning message')
    logger.error('an error message')
    logger.critical('a critical message')


    cfg = config.Config(filename=fs.CONFIG_FILE)
    cfg.load()
    cfg.set('lala', [1,22,3])
    cfg.save()

if __name__ == '__main__':
    boot_moosecat()
