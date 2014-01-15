#!/usr/bin/env python
# encoding: utf-8


QUERIES = [
    'a:Niveau b:Lose',
    'a:Akrea b:Lebenslinie',
    'a:Stones b:Licks',
    'a:Farin b:Lügen',
    'a:Varg b:Wolfskult',
    'a:Beatles b:1 ! Backstage',
    'a:Cranberries b:Argue',
    'a:Lordi',
    'a:Johnny b:Hits',
    'a:Schandmaul b:Anderswelt',
    'a:Extremo b:Blick',
    'a:Coppelius b:Zinnober',
    'a:Devildriver b:Devildriver',
    'a:Horizon b:Suicide',
    'a:Heaven b:Iconoclast',
    'a:Reiter b:Moral',
    'a:Finntroll b:Visor',
    'a:Finntroll b:Nattfödd',
    'a:Finntroll b:Blodsvept',
    'a:Nachtgeschrei b:Hoffnung*',
    'a:Suidakra b:Book',
    'a:Rhyne',
    'b:LIMBO ! b:Original',
    'a:Tanzwut (b:Labyrinth | b:Tanzwut)',
    'a:Instanz (b:Götter | b:Brachial* | b:Ewig)',
    'a:Monsters a:Men b:Animal',
    'a:Chubby b:Twist',
    'a:Clawfinger b:Zeros',
    'a:Cypres b:Death',
    'a:Toten a:Hosen b:II',
    'a:die (b:geräusch | b:Spendierhosen)',
    'b:Schnecken.Haus',
    'a:System b:Toxicity',
    'a:Tenacious b:Rize',
    'a:Rammstein b:Liebe',
    'a:Billy a:Talent b:III',
    'a:Biermösl b:Unterbayern',
    'a:Apocalyptica b:Apocalyptica',
    'a:Clint Mansell b:Moon',
    'a:Knorkator (b:Hasenchartbreaker | b:Knorkator)'
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

    query = ' | '.join('(' + sub + ')' for sub in QUERIES)
    print(query)

    with client.store.query(query, queue_only=False) as full_playlist:
        song_count = 0
        for song in full_playlist:
            src_path = os.path.join(sys.argv[1], song.uri)
            dst_path = os.path.join(sys.argv[2], song.uri)
            print('{:>70s} => {:>70s}'.format(
                os.path.basename(src_path),
                os.path.dirname(dst_path)
            ))

            song_count += 1
            if not '--dry' in sys.argv:
                os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                shutil.copyfile(src_path, dst_path)

        print('-- Copied {} files.'.format(song_count))
