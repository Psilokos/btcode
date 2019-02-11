cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)
thread_dep = dependency('threads')

libbtcode_sources = files(
    'btcode.c',
    'dct.c')

libbtcode = library('btcode', libbtcode_sources,
    include_directories: inc,
    dependencies: [m_dep, thread_dep])
