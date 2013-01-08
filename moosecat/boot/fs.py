#!/usr/bin/env python
# encoding: utf-8

import os
import logging


__all__ = ['CACHE_DIR', 'CONFIG_DIR', 'CONFIG_FILE']


LOGGER = logging.getLogger('boot.fs')
HOME_DIR = os.path.expanduser('~')


def _check_path(path):
    if not os.path.exists(path):
        LOGGER.warn('Path does not exist: ' + path)


def _check_or_mkdir(path):
    if not os.path.exists(path):
        os.mkdir(path)
        _check_path(path)


def _create_xdg_path(envar, endpoint):
    return os.environ.get(envar) or os.path.join(HOME_DIR, endpoint)


XDG_CONFIG_HOME = _create_xdg_path('XDG_CONFIG_HOME', '.config')
XDG_CACHE_HOME = _create_xdg_path('XDG_CACHE_HOME', '.cache')

_check_path(HOME_DIR)
_check_path(XDG_CONFIG_HOME)
_check_path(XDG_CACHE_HOME)

CONFIG_DIR = os.path.join(XDG_CONFIG_HOME, 'moosecat')
CACHE_DIR = os.path.join(XDG_CONFIG_HOME, 'moosecat')

_check_or_mkdir(CONFIG_DIR)
_check_or_mkdir(CACHE_DIR)

CONFIG_FILE = os.path.join(CONFIG_DIR, 'config.yaml')
LOG_FILE = os.path.join(CONFIG_DIR, 'app.log')
