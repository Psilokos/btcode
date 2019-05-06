project('btcode', 'c',
    version: '0.0.0',
    default_options: ['buildtype=release',
                      'b_ndebug=if-release'])

n_max_c_arg = '-DN_MAX=@0@'.format(get_option('N_MAX'))
add_project_arguments(n_max_c_arg, language: 'c')

btcode_inc_dir = include_directories('include')

subdir('src')
subdir('tools')
subdir('tests')
