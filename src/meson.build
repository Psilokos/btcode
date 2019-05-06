cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

libbtcode_inc_dir = include_directories('.')

gen_tables_sources = files(
    'gen_tables.c')

gen_tables = executable('gen_tables', gen_tables_sources,
    c_args: '-DCURDIR="' + meson.current_build_dir() + '/"',
    dependencies: m_dep)

libbtcode_gen_tables = custom_target('tables',
    command: gen_tables,
    output: ['tables.c', 'tables.h'])

libbtcode_sources = files(
    'btcode.c',
    'dct.c')

libbtcode = library('btcode', libbtcode_sources, libbtcode_gen_tables,
    include_directories: btcode_inc_dir,
    dependencies: m_dep)
