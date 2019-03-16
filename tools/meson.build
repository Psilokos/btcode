common_sources = files(
    'main.c')

btenc_sources = files(
    'enc_read_input.c',
    'enc_write_output.c')

btdec_sources = files(
    'dec_read_input.c',
    'dec_write_output.c')

btenc = executable('btenc', common_sources, btenc_sources,
    c_args: ['-DPROGRAM_NAME="btenc"', '-DBTCODE_ENCODE'],
    include_directories: inc,
    link_with: libbtcode)

btdec = executable('btdec', common_sources, btdec_sources,
    c_args: ['-DPROGRAM_NAME="btdec"', '-DBTCODE_DECODE'],
    include_directories: inc,
    link_with: libbtcode)
