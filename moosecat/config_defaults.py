#!/usr/bin/env python
# encoding: utf-8

'''
Default Configuration Values for the core.

These values do not include any plugin-specific things of course,
but they get added as default with the IConfigDefaults interface.

See the specific plugins for more information.
'''

CONFIG_DEFAULTS = {
    'verbosity': 3,
    'server_profile': {
        'default': {
            'display_name': 'Fallback Defaults',
            'host': 'localhost',
            'port':  6600,
            'timeout': 2.0,
            'addr': '127.0.0.1',
            'music_directory': None
        }
    },
    'server_profile_active': 'default',
    'metadata': {
        'providers': ['all'],
        'language_aware_only': False,
        'quality_speed_ratio': 1.0,
        'redirects': 3,
        'timeout': 10,
        'fuzzyness': 4,
        'verbosity': 0
    }
}
