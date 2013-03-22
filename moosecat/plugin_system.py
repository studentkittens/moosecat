# Yapsy Plugin System
from yapsy.PluginManager import PluginManager
from yapsy.IPlugin import IPlugin

# stdlib
import inspect
import logging
import importlib
import functools
import os


LOGGER = logging.getLogger('plugin_system')


def _get_interfaces_from_module():
    '''
    Find all interfaces defined in moosecat/plugins/__init__.py

    :returns: A dictionary: {name:class_object...}
    '''
    try:
        init_py = importlib.import_module('moosecat.plugins')
    except ImportError:
        LOGGER.exception('Cannot initialize plugin system')
        return {}

    result = {}

    # Get all classes of the init_py module
    for elem in inspect.getmembers(init_py, predicate=inspect.isclass):
        # klass is the actual class-object
        klass = elem[1]

        # Check if its an interface
        if issubclass(klass, IPlugin):
            try:
                # try to get it's name...
                result[klass.name] = klass
            except AttributeError:
                # ...or fallback to default - [0] is the name of the class.
                result[elem[0]] = klass

    return result


class PluginSystem:
    def __init__(self):
        'Initialize a new PluginSystem. Needs no arguments.'
        # Build the manager
        self._mngr = PluginManager(plugin_info_ext='plugin')

        # Tell it the default place(s) where to find plugins
        plugin_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'plugins')
        self._mngr.setPluginPlaces([plugin_path])

        # find the categories specified in moosecat/plugins/__init__.py
        self._categories = _get_interfaces_from_module()
        LOGGER.debug('Discovered following plugins categories: ' + ', '.join(self.list_categories()))

        # tell yapsy about our extra categories
        self._mngr.setCategoriesFilter(self._categories)

        try:
            self._mngr.collectPlugins()
        except SystemError as e:
            LOGGER.exception('Some plugin could not be loaded.')

        # Activate all loaded plugins
        # This is a TODO. We only want to load wanted plugins.
        # i.e. defined in the config.
        for pluginInfo in self._mngr.getAllPlugins():
            self._mngr.activatePluginByName(pluginInfo.name)

    def list_plugin_info(self):
        '''
        Get a list of PluginInfo objects.

        These contain additionnal metadata found in the .plugin files.

        See here for more information: http://yapsy.sourceforge.net/PluginInfo.html

        Useful if you want to realize a list of Plugins.

        :returns: A list of PluginInfo objects
        '''
        return self._mngr.getAllPlugins()

    def list_categories(self):
        'Get a string list of categories.'
        return self._categories.keys()

    def by_name(self, plugin_name):
        'Access a Plugin by its actual name - discouraged'
        return self._mngr.getPluginByName(plugin_name)

    @functools.lru_cache(maxsize=25)
    def category(self, name):
        '''
        Get a list of all Plugins of a Category.

        :name: The name of a category.
        :returns: A list of instances of a certain Plugin Classes.
        '''
        cat = [info.plugin_object for info in self._mngr.getPluginsOfCategory(name)]
        cat.sort(key=lambda pobj: pobj.priority(), reverse=True)
        return cat

    def first(self, name):
        '''
        Shortcut for ``psys.categories(name)[0]``

        Will return None if no plugins for this category.

        :name: A Category Name, Same as in ``category()``
        :returns: An Instance of a certain Plugin Class.
        '''
        cat = self.category(name)
        if len(cat) > 0:
            return cat[0]
