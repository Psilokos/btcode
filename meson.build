project('btcode', 'c',
    version: '0.0.1',
    default_options: ['buildtype=release',
                      'b_ndebug=if-release'])

blksz_c_arg = '-DBLKSZ=@0@'.format(get_option('BLKSZ'))
add_project_arguments(blksz_c_arg, language: 'c')

btcode_inc_dir = include_directories('include')

subdir('src')
subdir('tools')
subdir('tests')
