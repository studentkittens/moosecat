env = Environment()

# Try to load colorizer
# Make it in a try/except block
# so version missmatches don't hurt too much
try:
    from build.colorizer import colorizer
    col = colorizer()
    col.colorize(env)
except:
    print('Cannot load colorizer.py => No colored output.')

Export('env')

env.SConscript('lib/SConscript', variant_dir='bin', src_dir='src')
env.SConscript('test/SConscript', src_dir='test')
