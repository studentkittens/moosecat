#!/bin/sh
# TODO: this needs to be replaced by something proper.
wget http://waf.googlecode.com/files/waf-1.6.11 -O waf
python waf --version
python waf configure
python waf build
