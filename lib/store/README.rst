Overview
========

All source-code related to moosecat's client-side database is located here.
Moosecat mantains a mirror of MPD's database that is built up on startup. 
If there's already one on the disk it will use it and deserialize as long it is
up-to-date.

The reason why we do this is simple: Performance. This well indexed local cache
allows us to search songs in it with a well extended query language that is
easy to type. 


Files
=====

db.c

    Implements the user API, how the store reacts to changes, backups and client-changes.

db-dirs.c

    Implements caching and searching through the directory tree of MPD's database.
    Moosecat support vertical, horizontal and point selection:

        - Vertical Selection: Search the Tree by a Path-Pattern.
        - Horiztontal Selection: Select a certain depth of the Tree.
        - Point Selection: Combine both Horizontal and Vertical Selection.

db-macros.h

    Common Macros used mainly by db-private.c

db-operations.c
    
    Operations that are triggered in db.c (runinng plchanges and listallinfo)

db-private.c

    Actual Implementation of the Database. Here lives everything that has to do
    with SQL and SQLite. All the dirty stuff goes here.

db-query-parser.c

    A parser that enhances SQlite's FTS4 Query Syntax
    (http://www.sqlite.org/fts3.html#section_3) by an easier to type syntax.
    
    Example:

        Instead of: 

            (artist:Akr* OR album:Akr* OR title:Akr*) AND album:Lebenslinie

        You would type:

            Akr + b:Lebenslinie

db-settings.c

    Settings that can be passed to mc_store_create(). 

db-stored-playlists.c
    
    Implements everything related to stored playlists. SPLs are stored in a
    seperate table each on it's own. This code manages this and keeps those
    tables well updated. It can query Stored Playlists as fast as the normal
    Queue.

stack.c

    A container for the result. At it's heart this is a GPtrArray that grows 
    easily to hold many results (e.g. songs or playlists).

    One should note that many result-stacks can contain the same pointers,
    since moosecat will store a large stack of pointers to mpd_songs. Searching
    the Database just selects some of them and packs them into a new mc_Stack
    easily spoken.
