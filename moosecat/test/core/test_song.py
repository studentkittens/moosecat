import moosecat.core.moose as m
import unittest
import time


class TestSong(unittest.TestCase):
    def setUp(self):
        self.song = m.Song()
        self.song.test_begin()

    def tearDown(self):
        self.song = None

    def test_emptyAttrs(self):
        with self.assertRaises(ValueError):
            print(m.Song().artist)

    def test_setEmptyAttr(self):
        with self.assertRaises(ValueError):
            m.Song().queue_pos = 42

    def test_allProps(self):
        empty_song = m.Song()
        for attr in empty_song.__class__.__dict__.keys():
            str_attr = str(attr)
            if str_attr[0] != '_' and str_attr[:4] != 'test':
                with self.assertRaises(ValueError):
                    eval('empty_song.' + str_attr)

    def test_BeginDummy(self):
        self.song.test_feed('Artist', 'test')
        self.assertTrue(self.song.artist == 'test')
        self.assertTrue(self.song.album == '')

    def test_nonTagProps(self):
        # Is set to something by test_begin()
        self.assertTrue(self.song.uri != '')

        # See if we have an actual struct_time
        self.assertTrue(self.song.last_modified, time.struct_time)

        self.assertTrue(self.song.queue_pos == 0)
        self.assertTrue(self.song.queue_id == 0)
        self.assertTrue(self.song.duration == 0)

        self.song.queue_pos = 42
        self.assertTrue(self.song.queue_pos == 42)
