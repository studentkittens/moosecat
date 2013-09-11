#!/usr/bin/env python
# encoding: utf-8

import unittest
from moosecat.config import Config
from moosecat.server_profile import ServerProfile

TEST_CONFIG = '''
server_profile_active: default
server_profile:
    other:
        host: werkstatt
    default:
        host: localhost
        port: 6666
        timeout: 2.0
        display_name: GuessedThat
        music_directory: null
        addr: null
'''


class TestServerProfile(unittest.TestCase):
    def setUp(self):
        self.cfg = Config()
        self.profile = ServerProfile(self.cfg)

    def test_basics(self):
        self.cfg.load(TEST_CONFIG)
        self.assertEqual(self.profile.host, 'localhost')
        self.assertEqual(self.profile.port, 6666)
        self.assertEqual(self.profile.current_profile, 'default')

        self.profile.current_profile = 'other'

        self.assertEqual(self.profile.host, 'werkstatt')
        self.assertEqual(self.profile.port, 6666)
        self.assertEqual(self.profile.current_profile, 'other')

    def test_add_server(self):
        # Test setter shortly
        self.profile.host = 'werkzeug'
        self.assertEqual(self.profile.host, 'werkzeug')

        self.profile.add_server('zeroconf', 'Zeroconf', 'new_host', port=7777)
        self.assertEqual(self.profile.host, 'werkzeug')
        self.profile.current_profile = 'zeroconf'
        self.assertEqual(self.profile.host, 'new_host')
        self.assertEqual(self.profile.port, 7777)

        # Different port this time, try to overwrite it.
        self.profile.add_server('zeroconf', 'Zeroconf', 'new_host', port=9999)
        self.assertEqual(self.profile.port, 9999)
