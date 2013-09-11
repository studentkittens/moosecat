from yapsy.IPlugin import IPlugin


class IBasePlugin(IPlugin):

    name = 'Base'

    def priority(self):
        return 0


class IServerProfileProvider(IBasePlugin):
    'Plugin Interface for Plugins that want to provide connection data'

    name = 'ServerProfileProvider'

    def trigger_scan(self, server_profile):
        '''
        Find a server automatically by looking at various things.
        This function should not block.
        If servers were found.

        :param profiles: An instance of ServerProfile
        '''
        pass


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

    # TODO: This is redundant with priority()
    def get_priority(self):
        '''
        High priorites are shown more on top than lower (0-100)
        '''
        return 0
