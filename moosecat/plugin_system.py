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
        # Build the manager
        self.mngr = PluginManager(plugin_info_ext='plugin')

        # Tell it the default place(s) where to find plugins
        plugin_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'plugins')
        self.mngr.setPluginPlaces([plugin_path])

        # find the categories specified in moosecat/plugins/__init__.py
        categories = _get_interfaces_from_module()
        LOGGER.debug('Discovered following plugins categories: ' + ', '.join(categories.keys()))

        # tell yapsy about our extra categories
        self.mngr.setCategoriesFilter(categories)

        # Load all plugins
        try:
            self.mngr.collectPlugins()
        except SystemError as e:
            LOGGER.exception('Some plugin could not be loaded.')

        # Activate all loaded plugins
        # This is a TODO. We only want to load wanted plugins.
        # i.e. defined in the config.
        for pluginInfo in self.mngr.getAllPlugins():
            self.mngr.activatePluginByName(pluginInfo.name)

    def get_plugin_info(self):
        return self.mngr.getAllPlugins()

    @functools.lru_cache(maxsize=25)
    def category(self, name):
        cat = [info.plugin_object for info in self.mngr.getPluginsOfCategory(name)]
        cat.sort(key=lambda pobj: pobj.priority(), reverse=True)
        return cat

    def first(self, name):
        cat = self.category(name)
        if len(cat) > 0:
            return cat[0]
