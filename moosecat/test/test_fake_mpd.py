#!/usr/bin/env python
# encoding: utf-8

'''
In case you're wondering:

    Yes, this actually **is** the test of a test.
    But since fake_mpd.py isn't a very trivial piece of software, we'll
    better test it a bit more. Therefore you'll find a simple telnet client
    here that will query some of the server's responses.

    Im *not* crazy nor paranoid! :-P
'''


import telnetlib
import unittest


HOST = 'localhost'
PORT = 6666


class UtilMixin:
    def connect(self):
        self.client = telnetlib.Telnet(host=HOST, port=PORT)
        self.readline()

    def readline(self):
        return self.client.read_until(b'\n', timeout=1)

    def readall(self):
        return self.client.read_until(b'OK\n', timeout=1)

    def close(self):
        self.talk('close')
        self.client.close()

    def sendline(self, line):
        self.client.write(line.encode('utf-8') + b'\n')

    def talk(self, line):
        self.sendline(line)
        return self.readall()

    def talk_ok(self, line):
        self.assertEqual(self.talk(line), b'OK\n')

    def lookup_field(self, field):
        output = self.readall()
        for line in output.decode('utf-8').splitlines():
            if line == 'OK':
                break

            attr, value = line.split(':', maxsplit=1)
            if attr.strip() == field:
                return value.strip()

    def parse_ack(self, ack):
        '''
        Parse Example:

            b'ACK [2@0] {} unknowm command "grmblmbl!"\n'

        Results in:

            (error_id=2, command_list=0, current_cmd='',
             msg='unknown command "grmblmbl!"\n')
        '''
        splitted = ack.split()

        try:
            errid, cmdlist = [int(x) for x in splitted[1][1:-1].split(b'@', maxsplit=1)]
        except ValueError:
           errid, cmdlist = -1, -1

        return (errid, cmdlist, splitted[2][1:-1], b' '.join(splitted[3:]))


class TestFakeMPDBasics(unittest.TestCase, UtilMixin):
    def setUp(self):
        self.connect()

    def test_consume(self):
        self.talk_ok('consume on')
        self.sendline('status')
        self.assertEqual(self.lookup_field('consume'), '1')
        self.talk_ok('consume off')
        self.sendline('status')
        self.assertEqual(self.lookup_field('consume'), '0')

    def test_ok(self):
        self.talk_ok('ping')

    def test_ack(self):
        ack = self.talk('grmblmbl babumbl!')
        parsed_ack = self.parse_ack(ack)

        # error_id = 2
        self.assertEqual(parsed_ack[0], 2)

        # cmnd_list_num = 2
        self.assertEqual(parsed_ack[1], 0)

        # current command = b''
        self.assertEqual(parsed_ack[2], b'')

        # error_message = something bigger than 10 chars
        self.assertTrue(len(parsed_ack[3]) > 10)

    def tearDown(self):
        self.close()


class TestFakeMPDCommandList(unittest.TestCase, UtilMixin):
    def setUp(self):
        self.connect()

    def test_simple_list(self):
        self.sendline('command_list_begin')
        self.sendline('consume on')
        self.sendline('consume off')
        self.sendline('status')
        self.sendline('command_list_end')
        self.assertEqual(self.lookup_field('consume'), '0')

    def test_failure_list(self):
        self.sendline('command_list_begin')
        self.sendline('command_list_end')

    def test_empty_list(self):
        self.sendline('command_list_begin')
        self.sendline('command_list_end')
        self.assertEqual(self.readline(), b'OK\n')

    def tearDown(self):
        self.close()


if __name__ == '__main__':
    unittest.main()
