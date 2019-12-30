rm -rf /tmp/ffmpeg
mkdir /tmp/ffmpeg
./configure --prefix=/tmp/ffmpeg --extra-cflags="-mmacosx-version-min=10.13" --install-name-dir=@loader_path/../Frameworks --enable-pic --enable-shared --disable-static --disable-all --enable-avcodec --enable-decoder=h264 --enable-decoder=hevc --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox
make clean
make -j4
codesign -s "$SIGNING_IDENTITY" libavcodec/libavcodec.dylib
codesign -s "$SIGNING_IDENTITY" libavutil/libavutil.dylib
codesign -vv libavcodec/libavcodec.dylib
codesign -vv libavutil/libavutil.dylib
make install