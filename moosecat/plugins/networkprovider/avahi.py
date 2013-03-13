from moosecat.plugins import INetworkProvider


# Lacking Py3 Support for pybonjour is driving me nuts.


class AvahiNetworkProvider(INetworkProvider):
    def find(self):
        # TODO: Actually implement this someday
        return ('localhost', 6600)

    def priority(self):
        return 25
