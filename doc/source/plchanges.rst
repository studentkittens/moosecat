ASCII Art Style Explanation for plchanges
=========================================  

Non-Destructive (Moving):
-------------------------

:: 

   Swap B D and F H

   A       A
   B--\ /--D
   C   X   C
   D--/ \--B
   E       E
   F--\ /--H
   G   X   G
   H--/ \--F
   I       I
   ...   ...

::

   > plchanges $pref_plversion
   B
   D
   F
   H
   OK

.. note:: Only contains changed songs. Update: B D F H

Destructive:
------------

::

   Delete B C

   A       A
   B       D
   C   =>  E
   D       ... 
   E
   ... 

::

   > plchanges $pref_plversion
   D
   E
   OK

.. note:: Does not contain deleted songs. Update: **B** **C** D E

Destructive at End of Playlist:
-------------------------------

::

   Delete C D

   A     A
   B     B
   C  => ...
   D
   ...

::

  > plchanges $pref_plversion
  OK

.. note:: Nothing is returned. One is supposed to trim after queue_length. Update: **C** **D**

Conclusion:
-----------

* ``plchanges`` returns all changed songs.
* In the database and the stack, also deleted songs need to be updated, which are not shown by plchanges.
* Additional trimming needs to be done at the end of the playlist.
* If your client uses no database you only need to consider changed songs, no deleted ones.

Conclusion:
-----------

For each song ``plchanges`` returns, we first search the song with the pos/id in the db and set both to -1.
Then we find the file-uri of the new song and set the queue_pos and queue_idx accordingly.

Stack:
- Im an idiot. We'll just fucking mirror the database. one select over
  Excuse me, Im hitting the wall with my head.
