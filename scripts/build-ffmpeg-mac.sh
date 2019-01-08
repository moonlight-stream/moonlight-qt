./configure --extra-cflags="-mmacosx-version-min=10.11" --install-name-dir=@loader_path/../lib --enable-pic --enable-shared --disable-static --disable-all --enable-avcodec --enable-decoder=h264 --enable-decoder=hevc --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox
make clean
make -j4
codesign -s 45U78722YL libavcodec/libavcodec.dylib
codesign -s 45U78722YL libavutil/libavutil.dylib
codesign -vv libavcodec/libavcodec.dylib
codesign -vv libavutil/libavutil.dylib