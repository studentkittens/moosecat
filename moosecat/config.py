# encoding: utf-8

"""
Tools for loading and saving toplevel keys from a yaml file

.. moduleauthor:: serztle <serztle@googlemail.com>
"""

# stdlib
import logging
import collections

# pyaml
import yaml


_LOGGER  = logging.getLogger('config')

class Config:
    """A great self-explanatory configuration class.

    Loads and saves config params in a .yaml file

    """
    def __init__(self, filename='config.yaml'):
        """Constructor

        filename: name where the config is located
        """
        self._filename = filename
        self._data = {}

    def _resolve(self, name, default=None):
        'Resolve a config-urn to a key and its surrounding container'
        cont = self._data
        last = self._data
        for key in name.split('.'):
            try:
                if isinstance(last, collections.Mapping):
                    cont = last
                    last = last[key]
            except KeyError:
                return cont, None

        return cont, key

    def get(self, name, default=None):
        """get a configuration value by name

        :default: An optional default value may be passed (without None)
        :name: the toplevel key to search in the config
        :returns: a python object depending on the content of the .yaml file
        """
        container, last_key = self._resolve(name, default=default)
        return container.get(last_key, default)

    def __getitem__(self, idx):
        return self.get(idx)

    def __setitem__(self, idx, value):
        self.set(idx, value)

    def set(self, name, value):
        """set a configuration value by name

        :name: the toplevel key to search in the config
        :value: the python object that should be saved
        """
        container, last_key = self._resolve(name)
        if last_key is not None:
            container[last_key] = value

    def load(self):
        """Load the config from the .yaml file"""
        try:
            with open(self._filename, 'r') as f:
                self._data = yaml.load(f.read())
        except IOError:
            self._data = {}

        if not isinstance(self._data, collections.Mapping):
            self._data = {}

    def save(self):
        """Save the config to a .yaml file"""
        with open(self._filename, 'w') as f:
            f.write(yaml.dump(self._data, default_flow_style=False))

    def add_defaults(self, default_dict):
        'Add a dictionary with configuration values as default'
        self._data = {k: self._data.get(k, default_dict.get(k))
                for k in self._data.keys() | default_dict.keys()}
