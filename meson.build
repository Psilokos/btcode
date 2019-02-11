project('btcode', 'c',
    version: '0.0.1',
    default_options: ['buildtype=release',
                      'b_ndebug=if-release'])

n_max_c_arg = '-DBLKSZ=@0@'.format(get_option('BLKSZ'))
add_project_arguments(n_max_c_arg, language: 'c')

subdir('include')
subdir('src')
subdir('tools')
