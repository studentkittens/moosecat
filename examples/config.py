# encoding: utf-8

"""
Tools for loading and saving toplevel keys from a yaml file

Initial author (dotted keys get/set):

.. moduleauthor:: serztle <serztle@googlemail.com>

Default mechanism, profile, changed signal:

.. moduleauthor:: sahib <sahib@online.de>
"""

# stdlib
import collections

# pyaml
import yaml

# GObject (for signal emission/connect)
from gi.repository import GObject


def key_basename(key):
    'Returns "c" for key="a.b.c"'
    return key.split('.')[-1]


def key_dirname(key):
    'Returns "a.b" for key="a.b.c"'
    return '.'.join(key.split('.')[:-1])


def key_join(*paths):
    to_join = []
    for path in paths:
        if path.startswith('.'):
            path = path[+1:]
        if path.endswith('.'):
            path = path[:-1]
        to_join.append(path)
    return '.'.join(to_join)


class Config(GObject.Object):
    __gsignals__ = {
        'changed': (
            GObject.SIGNAL_RUN_FIRST | GObject.SIGNAL_DETAILED,
            None,
            (str, )
        )
    }

    """A great self-explanatory configuration class.

    Loads and saves config params in a .yaml file
    """
    def __init__(self):
        GObject.Object.__init__(self)
        self._data = {}

    def get(self, name, default=None, _next=None):
        """Get a configuration value by name

        :default: An optional default value may be passed (without None)
        :name: the toplevel key to search in the config
        :returns: a python object depending on the content of the .yaml file
        """
        if _next is None:
            _next = self._data

        if not isinstance(_next, collections.Mapping):
            raise ValueError('{name} is not subscriptable (endpoint?)'.format(
                name=name
            ))

        key, *leftover = name.split('.', maxsplit=1)
        if not leftover:
            # We're on the last part of the URN (no '.' left in URN)
            # And _next is a subscriptable item
            return _next.get(key, default)
        else:
            try:
                return self.get(leftover[0], default=default, _next=_next[key])
            except (KeyError, TypeError) as err:
                # TypeError happens if _next is not a collection.mapping
                # KeyError happens if a invalid path was given
                return

    def delete(self, name, _next=None):
        '''Delete a key and the corresponding value

        This is the same as: ::

            >>> del config['key']
        '''
        if _next is None:
            _next = self._data

        if not isinstance(_next, collections.Mapping):
            raise ValueError('{name} is not subscriptable (endpoint?)'.format(name=name))

        key, *leftover = name.split('.', maxsplit=1)
        if not leftover:
            del _next[key]
        else:
            self.delete(leftover[0], _next=_next[key])

    def __getitem__(self, idx):
        return self.get(idx)

    def __setitem__(self, idx, value):
        self.set(idx, value)

    def __delitem__(self, idx):
        self.delete(idx)

    def set(self, name, value, _next=None, original_name=None):
        """set a configuration value by name

        :name: the toplevel key to search in the config
        :value: the python object that should be saved
        """
        if _next is None:
            _next = self._data

        if not isinstance(_next, collections.Mapping):
            raise ValueError('{name} is not subscriptable (endpoint?)'.format(name=name))

        key, *leftover = name.split('.', maxsplit=1)
        if not leftover:
            _next[key] = value
            full_key = original_name or name
            self.emit('changed::' + full_key, full_key)
        else:
            self.set(leftover[0], value, _next.setdefault(key, {}), name)

    def load(self, content):
        """Load the config from the .yaml file"""
        self._data = yaml.load(content)
        if not isinstance(self._data, collections.Mapping):
            self._data = {}

    def convert(self):
        """Convert the config to a .yaml string"""
        return yaml.dump(self._data, default_flow_style=False)

    def save(self, path=None):
        """Save the config to a .yaml file"""
        if path is not None:
            with open(path, 'w') as f:
                f.write(self.convert())

    def add_defaults(self, default_dict, _next=None):
        'Add a dictionary with configuration values as default'
        if _next is None:
            _next = self._data

        for key in default_dict.keys():
            # default_dict has more information than us
            new_value = default_dict.get(key)

            # Repeat recursively
            if isinstance(new_value, collections.Mapping):
                if key not in _next:
                    _next[key] = {}
                if isinstance(_next[key], collections.Mapping):
                    self.add_defaults(new_value, _next[key])
            elif key not in _next:
                _next[key] = new_value

    def __str__(self):
        return str(self._data)


class Profile:
    '''
    A profile is a subset of the config which may have different states.

    As an example:

        There might be different presets of host, port for different Servers,
        which might be conviniently saved in different "Profiles". These
        Profiles have different names but store all the same keys.

    For a profile to work we expect a section in the config like this:

        servers:
            default:
                key1: value1
                key2: value2
            profile1:
                key1: value1
                key2: value3

    You would initialize the Profile like this: ::

        >>> p = Profile(config, root='servers')
        >>> p.current_profile
        'default'
        >>> p.get('key2')
        value2
        >>> p.current_profile = 'profile1'
        >>> p.get('key2')
        value3

    Otherwise the Profile acts like a Config object. It has get(), set(),
    __getitem__ and __setitem__ implemented. You can see a Profile therefore as
    a proxy to a Config object.

    .. note::

        A Profile never caches any data and operates always on the Config itself.
    '''
    def __init__(self, config, root, current_profile_key):
        '''
        :param config: The Config object to act as proxy for
        :param root: The root of the Profile, see the example above.
        :param current_profile_key: Config key to read the current profile from
        '''
        # Actual Values
        self._config, self.root, self.current_profile_key = config, root, current_profile_key
        self.add_profile(self.current_profile, overwrite=False)

    def __getitem__(self, key):
        return self.get(key)

    def __setitem__(self, key, value):
        self.set(key, value)

    ################
    #  Properties  #
    ################

    @property
    def root(self):
        'Return the currently set root'
        return self._root

    @root.setter
    def root(self, new_root):
        'Set a new root'
        value = self._config.get(new_root)
        if value is None:
            raise ValueError('No such config key:', new_root)
        self._root = new_root

    @property
    def current_profile_key(self):
        return self._current_profile_key

    @current_profile_key.setter
    def current_profile_key(self, new_key):
        self._current_profile_key = new_key
        self._current_profile = key_basename(new_key)

    @property
    def current_profile(self):
        'Currently used profile (might be "default")'
        return self._config.get(self.current_profile_key) or 'default'

    @current_profile.setter
    def current_profile(self, new_profile):
        new_key = self._build_base_path(new_profile)
        if not self._config.get(new_key):
            self.add_profile(new_profile)

        self._config.set(self.current_profile_key, new_profile)

    #############
    #  Methods  #
    #############

    def _build_key_path(self, key, profile=None):
        return '.'.join((self._root, profile or self.current_profile, key))

    def _build_base_path(self, profile_name):
        return '.'.join((self._root, profile_name))

    def get(self, key, default=None):
        'Returns <root>.<current_profile>.<key>'
        return self._config.get(self._build_key_path(key), default)

    def get_default(self, key):
        return self._config.get(self._build_key_path(key, 'default'))

    def set_default(self, key, value):
        return self._config.set(self._build_key_path(key, 'default'), value)

    def set(self, key, value):
        'Set <root>.<current_profile>.<key> to <value>'
        self._config[self._build_key_path(key)] = value

    def add_profile(self, profile_name, initial_value=None, overwrite=True):
        '''Adds a new profile to the config

        :param profile_name: Name of the profile
        :param initial_value: Value to set the profile to
        :param overwrite: If a profile with this name already exist overwrite it?
        '''
        path = self._build_base_path(profile_name)
        if overwrite or self._config[path] is None:
            self._config.set(path, initial_value or {})

    def delete_profile(self, profile_name):
        'Delete a profile from the config'
        del self._config[self._build_base_path(profile_name)]

        # If someone tried to delete the current profile
        # we switch to default to have at least one profile left
        if self.current_profile == profile_name:
            self.current_profile = 'default'


if __name__ == '__main__':
    def key_changed(cfg, key):
        print(key, '=>', cfg[key])

    config = Config()
    config.connect('changed::alpha.berta', key_changed)
    config.connect('changed::alpha.theta', key_changed)
    config.add_defaults({
        'alpha': {
            'berta': 10,
            'gamma': 30,
            'theta': 50
        }
    })

    print(config.get('alpha.berta'))
    config.set('alpha.berta', 20)
    config.set('alpha.gamma', 20)
    config.set('alpha.theta', 20)
