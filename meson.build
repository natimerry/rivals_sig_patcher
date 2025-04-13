project('rivals_sig_patcher','c',version: '0.1.0')

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

executable(
    'rivals_sig_patcher',
    sources,
    conf,
    include_directories: includes,
    win_subsystem: 'console'
)


shared_library(
    'rivals_sig_patcher',
    sources,
    conf,
    include_directories: includes,
    c_args: '-DPATCHER_DLL' # Define PATCHER_DLL when compiling as DLL
)