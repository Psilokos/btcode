check_steps_sources = files(
    'check_steps.c')

check_steps_objs = libbtcode.extract_objects(
    'dct.c')

check_steps = executable('check_steps', check_steps_sources,
    objects: check_steps_objs,
    include_directories: [btcode_inc_dir, libbtcode_inc_dir],
    dependencies: m_dep)

test('check_steps', check_steps)
