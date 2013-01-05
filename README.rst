========
MOOSECAT
========

It's moosetastic_!


.. image:: http://ompldr.org/vZ3l5aQ
   :scale: 25
   :align: center

#############################
OH WOW, *ANOTHER* MPD CLIENT?
#############################

There are only 240 (`I counted them`_) clients out there.
So here's our little contribution to get up to 256 clients.

FEATURES/GOALS
~~~~~~~~~~~~~~

Since moosecat is a full featured client, we only list the special features, 
that only a few other clients have. 

* **Database Caching.**
  
    **Fast** and intellgigent searching.

    A SQLite cache is built on startup, and is used instead of MPD's internal
    search commands. On the next startup it is loaded from disk which is a lot
    faster if the server is on a remote machine. \*

    To get most out of SQLite the built-in `FullTextSearch` Extension is used,
    which does quite some indexing, but delivers awesome performance.

    \* A Raspberry Pi in our case.

* **Playlist Manipulation in the style of** `mpdc`_.

    It is not exactly sure yet how this will be implemented, but 
    it should not be a large problem 

* **Good on Ressouces: Performance-critical stuff is written in C.**

    Even while the database sounds like poor memory usage, it is quite
    efficient! Only ~11MB is used for the database, which can be stored
    in memory or on disk - as the user likes. When moosecat shuts down, 
    it is also able to compress the database to ~2MB.

    Since a database is used the amount of network traffic is also significantly 
    lowered in normal usage. (On startup is a lot more of course ☻)

* **Plugins!**

    Moosecat sports an awesome Tag-based Plugin System, that is easy to use and understand.
    Every extended feature can be enabled or disabled as the user likes.

    External Plugins are possible via Python Eggs.

* **GUI-Agnostic.**

    All MPD related code and all plugins are located in the core. 
    The core itself is partly written in C (*libmoosecat*) and partly
    in Cython. A PyGtk-GUI or an Ncurses-UI might be written relatively
    easy and share a lot code with each else.

* **Strong Emphasis on Tests**

    The current goal is to develop a *mpc* client as first milestone, which
    is very well tested by automated unittests. 

    We even have a FakeMPD Server that is used to provide a clean counterpart
    for the client. It can also be used to startup moosecat without a proper server 
    anywhere.

* **Python**

    The fact that it's mostly written in Python is a feature itself. ☻

* **Strong Metadata Support**

    We do not mean Tagging here.

    We mean: Coverart, Lyrics, Artistbiographies, Related Links, Album reviews,
    Artistphotos and other stuff. 

    All served by `liblgyr`. (via `plyr`)


-------

~~~~~~~~~~~~~~
DOCUMENTATION
~~~~~~~~~~~~~~

The source is well annotated, but there's no user documentation yet.
Come back when it's done.

-------

~~~~~~~~~~~~
CONTRIBUTING
~~~~~~~~~~~~

1. Fork it.
2. Create a branch. (``git checkout -b my_markup``)
3. Commit your changes. (``git commit -am "Fixed it all."``)
4. Check if your commit message is good. (If not: ``git commit --amend``)
5. Push to the branch (``git push origin my_markup``)
6. Open a `Pull Request`.
7. Enjoy a refreshing ClubMate and wait.

-------

~~~~~~~
AUTHORS
~~~~~~~

Here's a list of people to blame:

===================================  ==========================  ========================================
*Christopher* **<sahib>** *Pahl*     https://github.com/sahib    He started it all. He also breaks it all
*Sebastian* **<serztle>** *Pahl*     https://github.com/serztle  YAML Config, Pluginsystem.
*Christoph* **<kitteh>** *Piechula*  https://github.com/qitta    Moral boosts, Idead for Pluginsystem 
===================================  ==========================  ========================================

.. _moosetastic: http://www.urbandictionary.com/define.php?term=moosetastic
.. _`I counted them`: http://mpd.wikia.com/wiki/Clients
.. _mpdc: http://nhrx.org/mpdc/
.. _FullTextSearch: http://www.sqlite.org/fts3.html
.. _`Pull Request`: http://github.com/studentkittens/moosecat/pulls
.. _libglyr: http://github.com/sahib/glyr
.. _plyr: http://github.com/sahib/python-glyr
