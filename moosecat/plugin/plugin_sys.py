#!/usr/bin/env python
# encoding: utf-8


"""
Tool(s) to manage plugins

.. moduleauthor:: serztle <serztle@googlemail.com>
"""

# Sehr sehr schön schon. Es fehlen noch ein paar weitere docstring und ein paar
# examples :-)

__author__ = 'serztle'


import sys
import types


def reload_package(root_module):
    package_name = root_module.__name__

    # get a reference to each loaded module
    loaded_package_modules = dict([
        (key, value) for key, value in sys.modules.items()
        if key.startswith(package_name) and isinstance(value, types.ModuleType)])

    # delete references to these loaded modules from sys.modules
    for key in loaded_package_modules:
        del sys.modules[key]

    # load each of the modules again;
    # make old modules share state with new modules
    for key in loaded_package_modules:
        print('loading %s' % key)
        newmodule = __import__(key)
        oldmodule = loaded_package_modules[key]
        oldmodule.__dict__.clear()
        oldmodule.__dict__.update(newmodule.__dict__)


class RegisterError(Exception):
    """Error for plugin register process"""
    pass


class Tag:
    """A struct for a tag"""
    def __init__(self):
        self.name = None
        self.description = None
        self.functions = None
        self.classes = None
        self.globals = None
        self.plugins = []


class Plugin:
    """A struct for a plugin"""
    def __init__(self):
        self.name = None
        self.import_path = None
        self.version = None
        self.priority = None
        self.on_load_func = None
        self.on_unload_func = None
        self.module = None
        self.tags = []


class PluginSystem:
    """A simple plugin system

    It`s possible to register tags. Tags defining a set of functions that must be implemented by plugins.
    Only if a plugin has all functions from a tag it is seen as plugin from that tag.
    Plugins register themselve. Individual plugins can loaded by tag or name, also called.
    """
    def __init__(self):
        """Constructor"""
        self._plugins = {}
        self._tags = {}
        self._loaded_plugins = {}
        self._loaded_plugins_by_tag = {}
        self._pdir_module = None

    def scan(self):
        """Load all plugins from the subfolder **plugins** """
        print('SCAN')
        imp_path = 'moosecat.plugin.plugins'
        if imp_path in sys.modules:
            print('RELOAD')
            reload_package(sys.modules[imp_path])
        else:
            __import__('moosecat.plugin.plugins')

    def register_tag(self, tag_description):
        """Register by description

        tag_description: Is a dict, for example:
            {'name': 'tag name',
             'description': 'A meaningful description,
             'functions': ['functions', 'that', 'must', 'be', 'implemented', 'by', 'plugins'],
             'classes': [],
             'globals': []
             }
        """
        tag_name = tag_description['name']
        if tag_name not in self._tags:
            tag = Tag()
            tag.name = tag_name
            tag.description = tag_description['description']
            tag.functions = tag_description['functions']
            tag.classes = tag_description['classes']
            tag.globals = tag_description['globals']
            tag.plugins = []

            self._tags[tag_name] = tag

            if tag_name not in self._loaded_plugins_by_tag:
                self._loaded_plugins_by_tag[tag_name] = []

    def register_plugin(self, name, import_path, version, priority=50, on_load_func=None, on_unload_func=None):
        """Register a plugin but doesn't load it

        name: plugin name
        import_path: python import-path of the module that contains the implemented functions
        version: version of the plugin
        priority: if more than one plugin is registered for one tag. they get called in descendant order of priority.
        on_load_func: when plugin gets loaded, the given function get called
        on_unload_func: when plugin gets unloaded, the given function get called
        """
        if name in self._plugins:
            raise RegisterError("Error while registering plugin (name already exists)")

        try:
            __import__(import_path)
            module = sys.modules[import_path]
        except(ImportError):
            raise RegisterError("Error while register plugin (Cannot import it)")

        # match all proper tags for this plugin
        matched_tags = []
        for tag in self._tags:
            check = 0
            for method in self._tags[tag].functions:
                if hasattr(module, method):
                    check = check + 1
            if check == len(self._tags[tag].functions):
                matched_tags.append(tag)

        if len(matched_tags) == 0:
            raise RegisterError("Error while registering plugin (No Tag could be guessed)")

        plugin = Plugin()
        plugin.name = name
        plugin.import_path = import_path
        plugin.version = version
        plugin.priority = priority
        plugin.on_load_func = on_load_func
        plugin.on_unload_func = on_unload_func
        plugin.module = module
        plugin.tags = matched_tags

        self._plugins[name] = plugin

        for tag in matched_tags:
            self._tags[tag].plugins.append(plugin)

    def load(self, name):
        if name not in self._loaded_plugins:
            if name in self._plugins:
                plugin = self._plugins[name]
                if plugin.on_load_func:
                    plugin.on_load_func()
                self._loaded_plugins[name] = plugin
                for tag in plugin.tags:
                    self._loaded_plugins_by_tag[tag].append(plugin)
                    # Ich versteh noch nicht ganz warum du es hier sortierst.
                    self._loaded_plugins_by_tag[tag] = sorted(self._loaded_plugins_by_tag[tag],
                                                              key=lambda plugin: plugin.priority,
                                                              reverse=True)
            else:
                raise LookupError("Plugin " + name + " doesn't exists")

    def unload(self, name):
        if name in self._loaded_plugins:
            plugin = self._plugins[name]
            if plugin.on_unload_func:
                plugin.on_unload_func()
            del self._loaded_plugins[name]
            for tag in plugin.tags:
                index = 0
                for plugin_list in self._loaded_plugins_by_tag[tag]:
                    if plugin_list == plugin:
                        del self._loaded_plugins_by_tag[tag][index]
                    index = index + 1

    def _load_unload_by_tag(self, tag_name, func):
        if tag_name in self._tags:
            for plugin in self._tags[tag_name].plugins:
                func(plugin.name)
        else:
            raise LookupError("Tag " + tag_name + " doesn`t exists")

    def load_by_tag(self, tag_name):
        self._load_unload_by_tag(tag_name, self.load)

    def unload_by_tag(self, tag_name):
        self._load_unload_by_tag(tag_name, self.unload)

    def __call__(self, tag_name):
        return self._loaded_plugins_by_tag.get(tag_name)

    def __getitem__(self, tag_name):
        return self._loaded_plugins_by_tag.get(tag_name, [None])[0]

psys = PluginSystem()
