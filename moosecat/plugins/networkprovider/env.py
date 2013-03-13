from moosecat.plugins import INetworkProvider

# os.environ
import os


class EnvNetworkProvider(INetworkProvider):
    def find(self):
        host = os.environ.get('MPD_HOST')
        port = os.environ.get('MPD_PORT')

        if host is not None:
            return (host, 6600 if port is None else port)

    def priority(self):
        return 50
