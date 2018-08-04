./configure --extra-cflags="-mmacosx-version-min=10.10" --enable-pic --enable-shared --disable-static --disable-all --enable-avcodec --enable-decoder=h264 --enable-decoder=hevc --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox
make clean
make -j4