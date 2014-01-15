#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import pickle
import sys

# Munin:
from munin.provider import \
    ArtistNormalizeProvider, \
    AlbumNormalizeProvider, \
    DiscogsGenreProvider


GENRE_PROVIDER = DiscogsGenreProvider()


# External:
import taglib


def write_genre_tag(music_path, genre_value):
    # Open the audio file via taglib:
    handle = taglib.File(music_path)

    # genre_value might be None. In this case just clear this tag.
    if genre_value is not None:
        # Existing genre tag gets overwritten.
        handle.tags['GENRE'] = genre_value
    elif 'GENRE' in handle.tags:
        del handle.tags['GENRE']

    failures = handle.save()
    if failures:
        print('Warning: could not write tags ({}) on: {}'.format(
            failures, music_path
        ))


def tag_music(data_mapping):
    for idx, ((artist, album), files) in enumerate(data_mapping.items()):
        print('#{}: {:<50} - {:<50}'.format(idx, artist, album))
        genre = GENRE_PROVIDER.do_process((artist, album))
        if genre is None:
            print('  Not found:', artist, album)
        else:
            print('  => ', genre)

        for path in files:
            print('  Tagging: ' + path)
            write_genre_tag(path, genre)


if __name__ == '__main__':
    # Stdlib:
    import os
    from collections import defaultdict

    # External:
    import moosecat.core

    client = moosecat.core.Client()
    client.connect(port=6600)
    client.store_initialize('/tmp')

    # Collect a whole-lot-of data from the database in this format:
    # {('artist', 'album'): set(['/home/a/foo.mp3', # '/home/piotr/blub.mp3'])}
    data_mapping = defaultdict(set)
    with client.store.query('*', queue_only=False) as full_playlist:
        for song in full_playlist:
            # Prefer album artist over the track artist
            artist = song.album_artist or song.artist
            full_path = os.path.join(sys.argv[1], song.uri)
            data_mapping[(artist, song.album)].add(full_path)

    tag_music(data_mapping)
    client.disconnect()
