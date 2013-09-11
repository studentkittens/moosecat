Metadata Retrieval
==================

Description
-----------

The Metadata-Retrieval-System is there to order metadata-items 
from `libglyr <http://www.github.com/sahib/glyr>`_ 
(via `plyr <http://www.github.com/sahib/python-glyr>`_).

The main class to work with is :py:class:`moosecat.metadata.Retriever`. 
There is by default already an instance of it in the ``g`` object called
``g.meta_retriever``. Plyr-Queries can be submitted via the ``submit()``
function, which takes a :py:class:`plyr.Query` and a callabble that will be called
once the query is done. 

Queries can be created by using ``plyr's`` API or by using these function:

.. autosummary::

    moosecat.metadata.configure_query
    moosecat.metadata.configure_query_by_current_song


The notify function will be called with excatly one argument, an instance of 
:py:class:`moosecat.metadata.Order`, which has a list of :class:`plyr.Cache`
in the ``results`` property.

Example
-------

Example code loosely taken from the GTK-UI's sidebar cover (fuzz removed): ::

    from moosecat.gtk.utils import plyr_cache_to_pixbuf
    from moosecat.metadata import configure_query_by_current_song
    from moosecat.boot import g

    def on_client_event(self, client, event):
        if event & Idle.PLAYER == 0:
            return

        g.meta_retriever.submit(
            self._on_item_retrieved, 
            configure_query_by_current_song('cover')
        )

    def on_item_retrieved(self, order):
        if len(order.results) > 0:
            cache, *_ = order.results

            # Do something clever with the stuff: Here we render a pixbuf.
            pixbuf = plyr_cache_to_pixbuf(cache, width=150, height=150)
            if pixbuf is not None:
                self._img_widget.set_from_pixbuf(pixbuf)


    if __name__ == '__main__':
        # Bootup omitted
        # ...
        g.client.signal_add('client-event', on_client_event)

Reference
---------

.. autoclass:: moosecat.metadata.Retriever
    :members:

.. autoclass:: moosecat.metadata.Order
    :members:

.. autofunction:: moosecat.metadata.configure_query
.. autofunction:: moosecat.metadata.configure_query_by_current_song
.. autofunction:: moosecat.metadata.update_needed
