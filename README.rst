========
MOOSECAT
========

.. figure:: https://raw.github.com/studentkittens/moosecat/master/screenshot.png
   :scale: 30%
   :align: center
   
   A Work-In-Progress Screenshot.


~~~~~~~~~~~~~~~~~~~~~~~~
*Another* MusicPD client?
~~~~~~~~~~~~~~~~~~~~~~~~

There are only 240 (`I counted them`_) clients out there. So what? Not even a
byte full. Sorry for the bad pun.

.. warning::

    It's not released and in a very flaky developement state currently.
    But most of the stuff works. No documentation yet, though.

-------

~~~~~~~~
FEATURES
~~~~~~~~

It's a full-fledged client, so here are the differences to other clients of this
class:

- `Auto Completion`_.
- `Fast Playlist Filtering`_.
- Intelligent Playlists via `libmunin`_ (coming soon).
- Metadata-Retrieval via `libglyr`_.
- A Powerful Query Syntax.
- GObject-based, written in C and Python.
- Zeroconf-Support.
- Uses a SQLite-cache for faster startup.
- Aims to have nice and extendable code.
- Gtk+-3.0 UI (in work) and ncurses interface (coming someday).

-------

~~~~~~~~~~~~
DEPENDENCIES
~~~~~~~~~~~~

C-Side-Stuff::

    sudo pacman -S libmpdclient avahi gtk3 python-gobject scons zlib

Python-Stuff::

    sudo pacman -S python-yaml cython 
    sudo pip install

-------

~~~~~~~~~~~~
CONTRIBUTING
~~~~~~~~~~~~

1. Fork it.
2. Create a branch. (``git checkout -b my_markup``)
3. Commit your changes. (``git commit -am "Fixed it all."``)
4. Check if your commit message is good. (If not: ``git commit --amend``)
5. Push to the branch (``git push origin my_markup``)
6. Open a `Pull Request`_.
7. Enjoy a refreshing ClubMate and wait.

-------

~~~~~~~
AUTHORS
~~~~~~~

Here's a list of people to blame:

===================================  ==========================  ===========================================
*Christopher* **<sahib>** *Pahl*     https://github.com/sahib    He started it all. He also breaks it all
*Sebastian* **<serztle>** *Pahl*     https://github.com/serztle  YAML Config, Pluginsystem, Metadata Chooser
*Christoph* **<kitteh>** *Piechula*  https://github.com/qitta    Moral boosts, Ideas for Pluginsystem 
===================================  ==========================  ===========================================

.. _I counted them`: http://mpd.wikia.com/wiki/Clients
.. _`Pull Request`: http://github.com/studentkittens/moosecat/pulls
.. _`Auto Completion`: https://dl.dropboxusercontent.com/u/12859833/completion.avi
.. _`Fast Playlist Filtering`: https://dl.dropboxusercontent.com/u/12859833/playlist_filter.avi
.. _`libmunin`: https://github.com/sahib/libmunin  
.. _`libglyr`: https://github.com/sahib/libglyr  
