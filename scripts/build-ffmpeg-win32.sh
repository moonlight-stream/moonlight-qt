./configure --toolchain=msvc --enable-cross-compile --enable-pic --enable-shared --disable-static --disable-all --enable-avcodec --enable-decoder=h264 --enable-decoder=hevc --enable-hwaccel=h264_dxva2 --enable-hwaccel=hevc_dxva2 --enable-hwaccel=h264_d3d11va --enable-hwaccel=hevc_d3d11va --enable-hwaccel=h264_d3d11va2 --enable-hwaccel=hevc_d3d11va2
make clean
make -j4