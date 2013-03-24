# encoding: utf-8

"""
A Simple Test for the config

.. moduleauthor:: serztle <serztle@googlemail.com>
"""

TEST_CONFIG = '''
active_profile: default
paths:
  cache_home: /home/sahib/.cache
  config_home: /home/sahib/.config
plugins:
  AvahiNetworkProvider:
    browse_timeout: 6666
profiles:
  default:
    host: localhost
    port: 6600
    timeout: 5.0
'''

import unittest
from moosecat.config import Config


class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = Config()
        self.cfg.load(TEST_CONFIG)

    def test_get(self):
        # Invalid:
        for urn in ['', '.', '..', '  ', 'profiles.', 'profiles..', 'profiles.default.host.']:
            self.assertEqual(self.cfg.get(urn), None)

        # Valid:
        self.assertEqual(self.cfg.get('profiles.default.port'), 6600)


    def test_add_default(self):
        # test non-existent key
        self.assertEqual(self.cfg.get('some_value'), None)
        self.assertEqual(self.cfg.get('some.value'), None)

        defaults = {
            'some_value': 42,
            'some': {
                'value': 21
            },
            'active_profile': 'Ha! Override it!'
        }

        self.cfg.add_defaults(defaults)
        self.assertEqual(self.cfg.get('some_value'), 42)
        self.assertEqual(self.cfg.get('some.value'), 21)
        self.assertEqual(self.cfg.get('active_profile'), 'default')

        # Manually overwrite values:
        self.cfg.set('some_value', 0)
        self.cfg.set('some.value', 1)

        self.assertEqual(self.cfg.get('some_value'), 0)
        self.assertEqual(self.cfg.get('some.value'), 1)
        self.cfg.add_defaults(defaults)
        self.assertEqual(self.cfg.get('some_value'), 0)
        self.assertEqual(self.cfg.get('some.value'), 1)

    def test_operators(self):
        self.assertEqual(self.cfg.get('profiles.default.host'), self.cfg['profiles.default.host'])
        self.assertEqual(self.cfg.get('profiles.default.port'), 6600)
        self.cfg['profiles.default.port'] = 6666
        self.assertEqual(self.cfg.get('profiles.default.port'), 6666)

    def test_save(self):
        try:
            self.cfg.save('')
        except FileNotFoundError:
            pass

        path = '/tmp/.test_config.save'
        self.cfg.save(path)
        with open(path, 'r') as f:
            content = f.read()
            self.assertEqual(content.strip(), TEST_CONFIG.strip())

    def test_malformed(self):
        # Pass in a HTML Document. Whyever.
        # Handle it gracefully, i.e. without exception
        self.cfg.load('''
                <html>
                    <head>
                        <title>Faulty Config</title>
                    </head>
                    <body>
                        <p>Config Body.</p>
                    </body>
                </html>
        ''')
        self.cfg.load('')
        self.cfg.load('''
            empty_key:
        ''')

if __name__ == '__main__':
    unittest.main()
