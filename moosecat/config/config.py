# encoding: utf-8

"""
Tools for loading and saving toplevel keys from a yaml file

.. moduleauthor:: serztle <serztle@googlemail.com>
"""


import yaml


class Config:
    """A great self-explanatory configuration class.

    Loads and saves config params in a .yaml file

    """
    def __init__(self, filename='config.yaml'):
        """Constructor

        filename: name where the config is located
        """
        self._loaded = False
        self._filename = filename
        self._data = {}

    def get(self, name, default=None):
        """get a configuration value by name

        default: An optional default value may be passed (without None)
        name: the toplevel key to search in the config
        returns: a python object depending on the content of the .yaml file
        """
        return self._data.get(name, default)

    def set(self, name, value):
        """set a configuration value by name

        name: the toplevel key to search in the config
        value: the python object that should be saved
        """
        self._data[name] = value

    def load(self):
        """Load the config from the .yaml file"""
        try:
            with open(self._filename, 'r') as f:
                self._data = yaml.load(f.read())
            self._loaded = True
        except IOError:
            self._data = {}
            self._loaded = False

        if not isinstance(self._data, dict):
            self._data = {}
            self._loaded = False

    def save(self):
        """Save the config to the .yaml file"""
        with open(self._filename, 'w') as f:
            f.write(yaml.dump(self._data, default_flow_style=False))
