from moosecat.plugins import INetworkProvider


class DefaultNetworkProvider(INetworkProvider):
    def find(self):
        return ('localhost', 6600)
