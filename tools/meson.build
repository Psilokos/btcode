common_sources = files(
    'main.c')

btenc_sources = files(
    'enc_parse_file.c')

btdec_sources = files(
    'dec_parse_file.c')

btenc = executable('btenc', common_sources, btenc_sources,
    c_args: ['-DPROGRAM_NAME="btenc"', '-DBTCODE_ENCODE'],
    include_directories: inc,
    link_with: libbtcode)

btdec = executable('btdec', common_sources, btdec_sources,
    c_args: ['-DPROGRAM_NAME="btdec"', '-DBTCODE_DECODE'],
    include_directories: inc,
    link_with: libbtcode)
