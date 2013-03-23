from moosecat.plugins import INetworkProvider, IConfigDefaults


# Lacking Py3 Support for pybonjour is driving me nuts.


class AvahiNetworkProvider(INetworkProvider, IConfigDefaults):
    def find(self):
        # TODO: Actually implement this someday
        return ('localhost', 6600)

    def priority(self):
        return 25

    def get_config_defaults(self):
        return {
            'browse_timeout': 6666
        }
