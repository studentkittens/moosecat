from moosecat.plugins import IServerProfileProvider
from moosecat.core import ZeroconfBrowser, ZeroconfState

import logging


class ZeroconfServerProvider(IServerProfileProvider):
    def trigger_scan(self, server_profile):
        self._server_profile = server_profile
        ZeroconfBrowser().register(self._on_server_found)
        # There will only happen stuff now if a mainloop is active.

    def _on_server_found(self, browser):
        logger = logging.getLogger('zeroconf')
        if browser.state == ZeroconfState.CHANGED:
            # Note: browser.server_list is a property and involves calculations
            server_list = browser.server_list
            logger.debug('#{} servers found.'.format(len(server_list)))

            for server in server_list:
                name, host = server['name'], server['host']
                self._server_profile.add_server(
                    profile_name='zeroconf_{0}_{1}'.format(name, host),
                    display_name='Zeroconf Provider ({0}/{1})'.format(name, host),
                    host=server['host'],
                    addr=server['addr'],
                    port=server['port']
                )
        elif browser.state == ZeroconfState.ALL_FOR_NOW:
            logger.debug('All for now I think!')
        elif browser.state == ZeroconfState.ERROR:
            logger.warning('Failure: ' + browser.error)

    def priority(self):
        return 25
