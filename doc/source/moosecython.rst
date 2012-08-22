Cython Wrapper
==============

.. _moosecython: 

What does it wrap?
------------------

All of ``libmoosecat``:

  .. py:module :: moosecat.mpd

  .. py:class :: Client 

     .. py:method :: __init__(host, port, timeout)

        :param host: Address of the mpd host (DNS Name or IPv4/6 addr.)
        :param port: Port of the mpd host.
        :param timeout: Max timeout to wait before cancelling network operations.
    
-------

  .. py:module :: moosecat.store

  .. py:class :: Store
     
     .. py:method :: __init__(location)

        Create a new database. If there is already a db it is opened, 
        if not a new one is created.

        :param location: Path to the database. 
  
  .. py:class :: Query
    
    .. py:method :: commit() 

       Execute the previously configured query.
       :returns: a list. Inner type depends on the options.
    

What does it provide additionally?
----------------------------------

  .. py:module :: moosecat.config

  .. py:function :: load(path=None)

     Load the config from *path*, if
     path is not given it try to read from: ::

       os.path.join(moosecat.paths.xdg_config_mc(), 'config.yaml')

     :param path: A valid file path.
     :returns: True on success.

-------

  .. py:module :: moosecat.paths

  .. py:function :: xdg_config_mc()

     Path to Moosecat's configuration dir, 
     according to XDG-Standard.

     Example: ``/home/user/.config/moosecat/``

     :returns: a valid directory path.

-------

  .. py:module :: moosecat.catellites 

  See :ref:`catellite_api` for the full description.

    .. py:function:: register(name, tags, version, priority, description='')
    .. py:function:: unload(name)
    .. py:function:: add_tag(tag_name, definition)
    .. py:data:: m
    .. py:data:: mf

-------

  .. py:module :: moosecat.init

  .. py:function :: bootstrap()

     Entry function.
