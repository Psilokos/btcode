check_steps_sources = files(
    'check_steps.c')

check_steps = executable('check_steps', check_steps_sources,
  # FIXME extract needed objs only
    objects: libbtcode.extract_all_objects(),
    include_directories: [btcode_inc_dir, libbtcode_inc_dir],
    dependencies: [m_dep, thread_dep])

test('check_steps', check_steps)
