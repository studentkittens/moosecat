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
from moosecat.config import Config, Profile


class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = Config()
        self.cfg.load(TEST_CONFIG)

    def test_get(self):
        # Invalid:
        for urn in ['', '.', '..', '  ', 'profiles.', 'profiles..', 'profiles.default.']:
            self.assertEqual(self.cfg.get(urn), None)

        with self.assertRaises(ValueError):
            self.cfg.get('profiles.default.host.')

        # Valid:
        self.assertEqual(self.cfg.get('profiles.default.port'), 6600)

    def test_set_get_combo(self):
        # Create new node not present in default
        self.cfg.set('some.new_value', 42)
        self.assertEqual(self.cfg.get('some.new_value'), 42)

        # This shouldn't work, new_value is an "endpoint"
        with self.assertRaises(ValueError):
            self.cfg.set('some.new_value.oh_crap', 3)

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
        self.cfg.set('some_value', 0)
        self.assertEqual(self.cfg.get('some_value'), 0)
        self.assertEqual(self.cfg.get('some.value'), 21)
        self.assertEqual(self.cfg.get('active_profile'), 'default')

        # Manually overwrite values:
        self.cfg.set('some.value', 1)

        self.assertEqual(self.cfg.get('some_value'), 0)
        self.assertEqual(self.cfg.get('some.value'), 1)
        self.cfg.add_defaults(defaults)
        self.assertEqual(self.cfg.get('some_value'), 0)
        self.assertEqual(self.cfg.get('some.value'), 1)

    def test_add_default(self):
        # Recursive merging of two dicts is difficutlt,
        # therefore stress this a bit.
        self.cfg.set('a.aa', 2)
        self.cfg.set('b.bb', 2)

        self.cfg.add_defaults({
            'a': {
                'aa': {
                    'aaa': 1
                }
            },
            'b': {
                'bb': 1
            },
            'c': {
                'cc': 1
            },
            'd': 1
        })
        self.cfg.set('a.aa', 2)

        self.assertEqual(self.cfg.get('a.aa'), 2)
        with self.assertRaises(ValueError):
            self.cfg.get('a.aa.aaa', None)
        self.assertEqual(self.cfg.get('b.bb'), 2)
        self.assertEqual(self.cfg.get('c.cc'), 1)
        self.assertEqual(self.cfg.get('d'), 1)

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


class TestProfile(unittest.TestCase):
    def setUp(self):
        self.cfg = Config()
        #self.cfg.load(TEST_CONFIG)
        self.cfg.add_defaults({
            'root': {},
            'current_profile': 'default'
        })
        self.profile = Profile(self.cfg, 'root', 'current_profile')

    def test_basic(self):
        # just to check if everthing still works if all profiles were deleted
        for i in range(1):
            # Test the basic functions
            value = {'value': 42}
            self.profile.set('value', 21)
            self.assertEqual(self.profile.get('value'), 21)
            self.profile.add_profile('hello_world', initial_value=value)
            self.profile.current_profile = 'hello_world'
            self.assertEqual(self.profile.current_profile, 'hello_world')
            self.assertEqual(self.profile.get('value'), 42)

            # This should switch to the default profile automaticallyy6
            self.profile.delete_profile('hello_world')
            self.assertEqual(self.profile.current_profile, 'default')
            self.assertEqual(self.profile.get('value'), 21)
            self.profile.delete_profile('default')
            self.assertEqual(self.profile.get('value'), None)

            # Now hijack the system a bit, change the config setting
            # directly and see if the profile reacts accordingly
            self.profile.add_profile('default', initial_value=value)
            self.profile.set('value', 21)
            self.profile.add_profile('another_one', initial_value={'ancient_gods': 3})
            self.cfg.set('current_profile', 'another_one')
            self.assertEqual(self.profile.get('ancient_gods'), 3)


if __name__ == '__main__':
    unittest.main()
