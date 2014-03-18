from moosecat.plugins import IServerProfileProvider

import os  # os.environ


class EnvServerProvider(IServerProfileProvider):
    def trigger_scan(self, server_profile):
        variables = ('MPD_HOST', 'MPD_PORT', 'MPD_MUSIC_DIR')
        host, port, path = (os.environ.get(var) for var in variables)

        if host is not None:
            try:
                port = int(port)
            # ValueError if invalid, or TypeError if None
            except (ValueError, TypeError):
                port = 6600

        server_profile.add_server(
            profile_name='env_vars',
            display_name='Env Variables',
            host=host,
            port=port,
            music_directory=path
        )

    def priority(self):
        return 50
