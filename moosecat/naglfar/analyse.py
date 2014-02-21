#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import logging

# Internal:
import moosecat.boot
from moosecat.boot import g

# External:
from munin.easy import EasySession
from munin.provider import PlyrLyricsProvider

# Fetch missing lyrics, or load them from disk.
# Also cache missed items for speed reasons.
LYRICS_PROVIDER = PlyrLyricsProvider(cache_failures=True)

def make_entry(song):
    # Hardcoded, Im sorry:
    full_uri = '/mnt/testdata/' + song.uri
    return song.uri, {
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
    }

if __name__ == '__main__':
    # pickle tends to hit python's recursionlimit
    import sys
    sys.setrecursionlimit(10000)

    # Bring up moosecat
    moosecat.boot.boot_base(verbosity=logging.DEBUG)
    g.client.disconnect()
    g.client.connect(port=6601)
    moosecat.boot.boot_metadata()
    moosecat.boot.boot_store()

    # Fetch the whole database into entries:
    entries = []
    with g.client.store.query('*', queue_only=False) as playlist:
        for song in playlist:
            entries.append(make_entry(song))

    # Instance a new EasySession and fill in the values.
    session = EasySession()
    with session.transaction():
        for uri, entry in entries:
            try:
                print('Processing:', entry['bpm'])
                session.mapping[session.add(entry)] = uri
            except:
                import traceback
                traceback.print_exc()

    # Save the Session to disk (~/.cache/libmunin/EasySession.gz)
    session.save()

    # Plot if desired.
    if '--plot' in sys.argv:
        session.database.plot()

    # Close the connection to MPD, save cached database
    moosecat.boot.shutdown_application()
