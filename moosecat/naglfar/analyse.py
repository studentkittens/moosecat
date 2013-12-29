#!/usr/bin/env python
# encoding: utf-8


# Stdlib:
import logging

# Internal:
from moosecat.boot import g, boot_base, boot_store, boot_metadata, shutdown_application

# External:
from munin.easy import EasySession
from munin.provider import PlyrLyricsProvider


LYRICS_PROVIDER = PlyrLyricsProvider()


def retrieve_lyrics(artist, title):
    return LYRICS_PROVIDER.do_process((artist, title))


def make_entry(song):
    # TODO
    full_uri = '/home/sahib/hd/music/clean/' + song.uri
    return (song.uri, {
        'artist': song.artist,
        'album': song.album,
        'title': song.title,
        'genre': song.genre,
        'bpm': full_uri,
        'moodbar': full_uri,
        'lyrics': retrieve_lyrics(song.artist, song.title)
    })

if __name__ == '__main__':
    import sys

    # Bring up the core!
    boot_base(verbosity=logging.DEBUG)
    boot_metadata()
    boot_store()

    entries = []
    with g.client.store.query(sys.argv[1], queue_only=False) as playlist:
        for song in playlist:
            entries.append(make_entry(song))

    session = EasySession()
    with session.transaction():
        for uri, entry in entries:
            try:
                print('Processing:', entry['bpm'])
                session.mapping[session.add(entry)] = uri
            except:
                import traceback
                traceback.print_exc()

    session.save()
    session.database.plot()
    shutdown_application()
