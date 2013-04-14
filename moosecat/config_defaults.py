#!/usr/bin/env python
# encoding: utf-8

'''
Default Configuration Values for the core.

These values do not include any plugin-specific things of course,
but they get added as default with the IConfigDefaults interface.

See the specific plugins for more information.
'''

CONFIG_DEFAULTS = {
    'profiles': {
        'default': {
            'host': 'localhost',
            'port': 6600,
            'timeout': 2.0,
            'password': None
        }
    },
    'active_profile': 'default',
    'verbosity': 3
}
