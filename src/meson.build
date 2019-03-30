cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

libbtcode_inc_dir = include_directories('.')

libbtcode_sources = files(
    'btcode.c',
    'dct.c')

libbtcode = library('btcode', libbtcode_sources,
    include_directories: btcode_inc_dir,
    dependencies: m_dep)
