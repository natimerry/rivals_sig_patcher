project('rivals_sig_patcher', 'c', version: '0.3.0')

sources = [
    'src/game.c',
    'src/patcher.c',
    'src/main.c',
    'src/msg.c',
]

includes = include_directories('include')

cfg_data = configuration_data()
cfg_data.set('version', meson.project_version())

conf = configure_file(input: 'include/config.h.in', output: 'config.h', configuration: cfg_data)

is_release = get_option('buildtype') == 'release'

c_args = []

if not is_release
    c_args += ['-DTEST_BUILD']
endif

executable(
    'rivals_sig_patcher',
    sources,
    conf,
    include_directories: includes,
    win_subsystem: 'console',
    c_args: c_args,
)

shared_library(
    'rivals_sig_patcher',
    sources,
    conf,
    include_directories: includes,
    c_args: c_args + ['-DPATCHER_DLL'],
)