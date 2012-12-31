import moosecat.core as m
import unittest


stats_response = '''artists: 644
albums: 1326
songs: 16760
uptime: 407526
playtime: 285000
db_playtime: 4609008
db_update: 134909661
'''


class TestStats(unittest.TestCase):
    def setUp(self):
        self.stats = m.Statistics()
        self.stats.test_begin()
        for line in stats_response.splitlines():
            self.stats.test_feed(*line.split(':'))

    def test_attributes(self):
        self.assertTrue(self.stats.number_of_artists == 644)
        self.assertTrue(self.stats.number_of_albums == 1326)
        self.assertTrue(self.stats.number_of_songs == 16760)
        self.assertTrue(self.stats.uptime == 407526)
        self.assertTrue(self.stats.playtime == 285000)
        self.assertTrue(self.stats.db_update_time == 134909661)
        self.assertTrue(self.stats.db_playtime == 4609008)

    def test_emptyAttributes(self):
        empty = m.Statistics()
        self.assertRaises(ValueError, lambda: empty.number_of_artists)
        self.assertRaises(ValueError, lambda: empty.number_of_albums)
        self.assertRaises(ValueError, lambda: empty.number_of_songs)
        self.assertRaises(ValueError, lambda: empty.uptime)
        self.assertRaises(ValueError, lambda: empty.playtime)
        self.assertRaises(ValueError, lambda: empty.db_update_time)
        self.assertRaises(ValueError, lambda: empty.db_playtime)

    def tearDown(self):
        self.stats = None
