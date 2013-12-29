#!/usr/bin/env python
# encoding: utf-8

API_SEARCH = "http://api.discogs.com/database/search?type=release&q={artist}"


import json
import pickle
import sys

from urllib.request import urlopen
from urllib.parse import quote


from munin.provider.normalize import ArtistNormalizeProvider, AlbumNormalizeProvider


# External:
from pyxdameraulevenshtein import normalized_damerau_levenshtein_distance as levenshtein
import taglib


def write_genre_tag(music_path, genre_value):
    handle = taglib.File(music_path)
    handle.tags['GENRE'] = genre_value or ''
    failures = handle.save()
    if failures:
        print('Warning: could not write tags ({}) on: {}'.format(
            failures, music_path
        ))


def find_genre_via_discogs(artist, album):
    genre_set, style_set = set(), set()

    api_root = API_SEARCH.format(artist=quote(artist))
    html_doc = urlopen(api_root).read().decode('utf-8')
    json_doc = json.loads(html_doc)

    artist_normalizer = ArtistNormalizeProvider()
    album_normalizer = AlbumNormalizeProvider()

    artist, *_ = artist_normalizer.do_process(artist)
    album, *_ = album_normalizer.do_process(album)

    def find_right_genre(persist_on_album):
        for item in json_doc['results']:
            if 'style' not in item:
                continue

            # print('   ', item['title'], item['genre'], item['style'])
            remote_artist, remote_album = item['title'].split(' - ', maxsplit=1)
            remote_artist, *_ = artist_normalizer.do_process(remote_artist)
            remote_album, *_ = album_normalizer.do_process(remote_album)

            if levenshtein(artist, remote_artist) > 0.5:
                continue

            if persist_on_album and levenshtein(album, remote_album) > 0.5:
                continue

            genre_set.update(item['genre'])
            style_set.update(item['style'])

    find_right_genre(True)
    if not (genre_set or style_set):
        find_right_genre(False)

    if not (genre_set or style_set):
        return None

    return (' / '.join(('; '.join(genre_set), ', '.join(style_set))))


def tag_music(data_mapping):
    CACHE_PATH = '/tmp/genre.cache'
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
    import os
    from collections import defaultdict

    import moosecat.core as m

    client = m.Client()
    client.connect()
    client.store_initialize('/tmp')

    data_mapping = defaultdict(set)
    with client.store.query('*') as full_playlist:
        for song in full_playlist:
            artist = song.album_artist or song.artist
            full_path = os.path.join(sys.argv[1], song.uri)
            data_mapping[(artist, song.album)].add(full_path)

    tag_music(data_mapping)
