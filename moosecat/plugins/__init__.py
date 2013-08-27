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


###########################################################################
#                            Gtk UI Interfaces                            #
###########################################################################

class IGtkBrowser(IBasePlugin):
    '''
    Implement this interface to be added in the GTK's UI as a Browser.
    '''

    name = 'GtkBrowser'

    def do_build(self):
        pass

    def get_root_widget(self):
        'Return the root widget of your browser window.'
        pass

    def get_browser_name(self):
        'Get the name of the browser (displayed in the sidebar)'
        pass

    def get_browser_icon_name(self):
        '''
        Get the icon-name to be displayed along the name.
        For example: GTK.STOCK_CDROM.
        '''
        pass

    def get_priority(self):
        '''
        High priorites are shown more on top than lower (0-100)
        '''
        return 0
