#!/usr/bin/env python
# encoding: utf-8


# Stdlib:
import logging

# Internal:
from moosecat.boot import g, boot_base, boot_store, boot_metadata, shutdown_application

# External:
from munin.easy import EasySession
from munin.provider import PlyrLyricsProvider


LYRICS_PROVIDER = PlyrLyricsProvider(cache_failures=True)


def make_entry(song):
    # TODO
    full_uri = '/mnt/testdata/' + song.uri
    return (song.uri, {
        'artist': song.artist,
        'album': song.album,
        'title': song.title,
        'genre': song.genre,
        'bpm': full_uri,
        'moodbar': full_uri,
        'rating': None,
        'date': song.date,
        'lyrics': LYRICS_PROVIDER.do_process((
            song.album_artist or song.artist, song.title
        ))
    })

if __name__ == '__main__':
    import sys
    sys.setrecursionlimit(10000)

    # Bring up the core!
    boot_base(verbosity=logging.DEBUG)
    g.client.disconnect()
    g.client.connect(port=6601)
    boot_metadata()
    boot_store()

    entries = []
    with g.client.store.query('*', queue_only=False) as playlist:
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

    # session.database.plot()
    shutdown_application()
