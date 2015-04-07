# Stdlib:
import cmd

# Internal:
from gi.repository import Moose


from gi.repository import GObject


class InternalLogCatcher(GObject.Object):
    __gsignals__ = {
        'log-message': (
            GObject.SIGNAL_RUN_FIRST,
            None,
            (str, str, str)
        )
    }

def log_message(catcher, domain, level, msg):
    pass


catcher = InternalLogCatcher()
catcher.connect('log-message', log_message)
Moose.misc_catch_external_logs(catcher)

client = Moose.CmdClient()

client.connect_to('localhost', 6600, 100)
store = Moose.Store.new(client)
completion = store.get_completion()


class CompletionShell(cmd.Cmd):
    intro = 'Type a part of an artist and press enter:'
    prompt = '>>> '

    def default(self, line):
        pass

    def do_complete(self, line):
        guess = completion.lookup(Moose.TagType.ARTIST, line.strip())
        print(line, '->', guess)

    def do_exit(self, _):
        print('[quitting]')
        return True

    def precmd(self, line):
        if line == 'EOF':
            return 'exit'

        self.do_complete(line)
        return ''

try:
    shell = CompletionShell()
    shell.cmdloop()
except KeyboardInterrupt:
    print('[interrupted]')
