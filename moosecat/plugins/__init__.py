from yapsy.IPlugin import IPlugin


class IBasePlugin(IPlugin):

    name = 'Base'

    def priority(self):
        return 0


class INetworkProvider(IBasePlugin):
    'Plugin Interface for Plugins that want to provide connection data'

    name = 'NetworkProvider'

    def find(self):
        '''
        Find a server automatically.

        :returns: a tuple: (host:str, port:int, timeout:float)
        '''
        return ('localhost', 6600, 2.0)

class IConfigDefaults(IBasePlugin):
    '''
    Implement this Interface if your Plugin needs to store Configuration Values.
    '''

    name = 'ConfigDefaults'

    def get_config_defaults(self):
        'Return a dictionary that gets passed to Config.add_defaults()'
        return {}
