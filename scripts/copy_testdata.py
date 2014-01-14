#!/usr/bin/env python
# encoding: utf-8


QUERIES = [
    'a:Niveau b:Lose',
    'a:Akrea',
    'a:Stones b:Licks',
    'a:Farin b:Lügen',
    'a:Varg b:Wolfskult'
]


if __name__ == '__main__':
    # Stdlib:
    import os
    import sys
    import shutil

    # Internal
    import moosecat.core

    client = moosecat.core.Client()
    client.connect(port=6600)
    client.store_initialize('/tmp')

    query = '|'.join('(' + sub + ')' for sub in QUERIES)

    with client.store.query(query, queue_only=False) as full_playlist:
        for song in full_playlist:
            src_path = os.path.join(sys.argv[1], song.uri)
            dst_path = os.path.join(sys.argv[2], song.uri)
            print('{:>70s} => {:>70s}'.format(
                os.path.basename(src_path),
                os.path.dirname(dst_path)
            ))
            if not '--dry' in sys.argv:
                os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                shutil.copyfile(src_path, dst_path)
