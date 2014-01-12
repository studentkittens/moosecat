#!/usr/bin/env python
# encoding: utf-8

API_SEARCH_URL = "http://api.discogs.com/database/search?type=release&q={artist}"


import json
import pickle
import sys

from collections import Counter
from urllib.request import urlopen
from urllib.parse import quote


from munin.provider.normalize import \
    ArtistNormalizeProvider, \
    AlbumNormalizeProvider

# External:
from pyxdameraulevenshtein import normalized_damerau_levenshtein_distance as \
    levenshtein
import taglib


def filter_crosslinks(genre_set, style_set):
    for key in genre_set:
        if key in style_set:
            del style_set[key]


def find_genre_via_discogs(artist, album):
    genre_set, style_set = Counter(), Counter()

    # Get the data from discogs
    api_root = API_SEARCH_URL.format(artist=quote(artist))
    html_doc = urlopen(api_root).read().decode('utf-8')
    json_doc = json.loads(html_doc)

    # Normalize the input artist/album
    artist_normalizer = ArtistNormalizeProvider()
    album_normalizer = AlbumNormalizeProvider()
    artist, *_ = artist_normalizer.do_process(artist)
    album, *_ = album_normalizer.do_process(album)

    def find_right_genre(persist_on_album):
        for item in json_doc['results']:
            # Some artist items have not a style in them.
            # Skip these items.
            if 'style' not in item:
                continue

            # Get the remote artist/album from the title, also normalise them.
            remote_artist, remote_album = item['title'].split(' - ', maxsplit=1)
            remote_artist, *_ = artist_normalizer.do_process(remote_artist)
            remote_album, *_ = album_normalizer.do_process(remote_album)

            # Try to outweight spelling errors, or small
            # pre/suffixes to the artist. (i.e. 'the beatles' <-> beatles')
            if levenshtein(artist, remote_artist) > 0.5:
                continue

            # Same for the album:
            if persist_on_album and levenshtein(album, remote_album) > 0.5:
                continue

            # Remember the set of all genres and styles.
            genre_set.update(item['genre'])
            style_set.update(item['style'])

        filter_spam(genre_set)
        filter_spam(style_set)
        filter_crosslinks(genre_set, style_set)

    def filter_spam(counter):
        if not counter:
            return

        avg = sum(counter.values()) // len(counter)
        for key in list(counter):
            if counter[key] < avg:
                del counter[key]

    find_right_genre(True)
    if not (genre_set or style_set):
        # Lower the expectations, just take the genre of
        # all known albums of this artist, if any:
        print('  -- exact album not found.')
        find_right_genre(False)

    # Still not? Welp.
    if not (genre_set or style_set):
        return None

    # Bulid a genre string that is formatted this way:
    #  genre1; genre2 [;...] / style1, style2, style3 [,...]
    #  blues, rock / blues rock, country rock, christian blues
    return ' / '.join((
        ', '.join(k for k, v in genre_set.most_common(3)),
        ', '.join(k for k, v in style_set.most_common(4))
    ))


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
    CACHE_PATH = '/tmp/genre.cache.2'
    try:
        with open(CACHE_PATH, 'rb') as handle:
            genre_cache = pickle.load(handle)
    except OSError:
        genre_cache = {}

    try:
        for idx, ((artist, album), files) in enumerate(data_mapping.items()):
            print('#{}: {:<50} - {:<50}'.format(idx, artist, album))
            if (artist, album) not in genre_cache:
                genre = find_genre_via_discogs(artist, album)
                if genre is None:
                    print('  Not found:', artist, album)
                else:
                    print('  => ', genre)
                    genre_cache[(artist, album)] = genre

                for path in files:
                    print('  Tagging: ' + path)
                    write_genre_tag(path, genre)
    finally:
        with open(CACHE_PATH, 'wb') as handle:
            pickle.dump(genre_cache, handle)


if __name__ == '__main__':
    # Stdlib:
    import os
    from collections import defaultdict

    # External:
    import moosecat.core

    client = moosecat.core.Client()
    client.connect(port=6601)
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
