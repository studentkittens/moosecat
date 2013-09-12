from moosecat.config import Profile
from functools import partial


class ServerProfile(Profile):
    '''
    A special profile that manages the different server profiles.

    In the bigger picture there is a ServerProfileProvider plugin-interface
    for plugins that want to provide connection data. Those plugins add servers
    to the profile by calling add_server with some of the data (not all data is required).
    '''
    def __init__(self, config):
        config.add_defaults({
            'server_profile_active': 'default',
            'server_profile': {}
        })
        Profile.__init__(
                self, config,
                root='server_profile',
                current_profile_key=config.get('server_profile_active')
        )

        # Fill the properties by some meta-programming
        self._create_properties()

    def add_server(self, profile_name, display_name, host=None, addr=None, port=6600, timeout=2.0, music_directory=None, password=None):
        info = {
            key: value for key, value in dict(
                host=host or addr,
                addr=addr or host,
                port=port,
                display_name=display_name,
                timeout=timeout,
                music_directory=music_directory
            ).items() if value is not None
        }

        Profile.add_profile(self, profile_name, overwrite=True, initial_value=info)

    def trigger_scan(self, psys):
        for plugin in psys.category('ServerProfileProvider'):
            plugin.trigger_scan(self)

    ###########################################################################
    #                       Properties for convinience                        #
    ###########################################################################

    class _ConfigProperty:
        def __init__(self, key, doc):
            self._key = key
            self._doc = doc

        def __get__(self, instance, owner):
            first_try = instance.get(self._key)
            if first_try is None:
                return instance.get_default(self._key)
            return first_try

        def __set__(self, instance, value):
            instance.set(self._key, value)

        def __doc__(self):
            return self._doc

    def _create_properties(self):
        properties = ['music_directory', 'host', 'port', 'timeout', 'addr', 'display_name']
        for property_name in properties:
            setattr(ServerProfile, property_name, self._ConfigProperty(
                key=property_name,
                doc='Return {p} (Same as .get("{p}"))'.format(p=property_name)
            ))
