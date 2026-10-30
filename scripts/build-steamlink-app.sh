BUILD_CONFIG="release"
QT_514_COMMIT="609d4aaccb503298e7fa9cef45e0ddc4c4afd63c"

fail()
{
	echo "$1" 1>&2
	exit 1
}

if [ "$STEAMLINK_SDK_PATH" == "" ]; then
  fail "You must set STEAMLINK_SDK_PATH to build for Steam Link"
fi

BUILD_ROOT=$PWD/build
SOURCE_ROOT=$PWD
BUILD_FOLDER=$BUILD_ROOT/build-$BUILD_CONFIG
DEPLOY_FOLDER=$BUILD_ROOT/deploy-$BUILD_CONFIG
INSTALLER_FOLDER=$BUILD_ROOT/installer-$BUILD_CONFIG

if [ -n "$CI_VERSION" ]; then
  VERSION=$CI_VERSION
else
  VERSION=`cat $SOURCE_ROOT/app/version.txt`
fi

echo Updating dependencies
python3 $SOURCE_ROOT/setup-deps.py

echo Cleaning output directories
rm -rf $BUILD_FOLDER
rm -rf $DEPLOY_FOLDER
rm -rf $INSTALLER_FOLDER
mkdir $BUILD_ROOT
mkdir $BUILD_FOLDER
mkdir $DEPLOY_FOLDER
mkdir $INSTALLER_FOLDER

echo Initializing Steam Link SDK
source $STEAMLINK_SDK_PATH/setenv.sh || fail "SL SDK initialization failed!"

SDL_BUILD_ROOT=$BUILD_ROOT/steamlink-sdl
SDL_INSTALL_ROOT=$SDL_BUILD_ROOT/install

if [ ! -d "$SOURCE_ROOT/deps/SDL" ] || [ ! -d "$SOURCE_ROOT/deps/SDL_ttf" ]; then
  fail "Missing SDL3 sources. Check out ReenigneArcher/SDL and SDL_ttf in deps/SDL and deps/SDL_ttf."
fi

echo Building SDL3 for Steam Link
cmake -S "$SOURCE_ROOT/deps/SDL" -B "$SDL_BUILD_ROOT/SDL" \
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL_ROOT" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF \
  -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
  -DSDL_PULSEAUDIO=OFF \
  -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_INSTALL_TESTS=OFF \
  -DSDL_INSTALL_DOCS=OFF -DSDL_HIDAPI_LIBUSB=OFF || fail "SDL3 configuration failed!"
cmake --build "$SDL_BUILD_ROOT/SDL" -j$(nproc) || fail "SDL3 build failed!"
cmake --install "$SDL_BUILD_ROOT/SDL" || fail "SDL3 install failed!"

echo Building SDL_ttf for Steam Link
cmake -S "$SOURCE_ROOT/deps/SDL_ttf" -B "$SDL_BUILD_ROOT/SDL_ttf" \
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL_ROOT" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL3_DIR="$SDL_INSTALL_ROOT/lib/cmake/SDL3" \
  -DBUILD_SHARED_LIBS=ON -DSDLTTF_INSTALL=ON -DSDLTTF_VENDORED=ON \
  -DSDLTTF_HARFBUZZ=OFF -DSDLTTF_PLUTOSVG=OFF \
  -DSDLTTF_SAMPLES=OFF -DSDLTTF_TESTS=OFF || fail "SDL_ttf configuration failed!"
cmake --build "$SDL_BUILD_ROOT/SDL_ttf" -j$(nproc) || fail "SDL_ttf build failed!"
cmake --install "$SDL_BUILD_ROOT/SDL_ttf" || fail "SDL_ttf install failed!"

echo Configuring the project
export STEAMLINK_SDL3_PREFIX="$SDL_INSTALL_ROOT"
pushd $BUILD_FOLDER
qmake $SOURCE_ROOT/moonlight-qt.pro QMAKE_CFLAGS_ISYSTEM= || fail "Qmake failed!"
popd

echo Compiling Moonlight in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make -j$(nproc) $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
popd

echo Creating app bundle
mkdir -p $DEPLOY_FOLDER/steamlink/apps/moonlight/bin
cp $BUILD_FOLDER/app/moonlight $DEPLOY_FOLDER/steamlink/apps/moonlight/bin/ || fail "Binary copy failed!"
mkdir -p $DEPLOY_FOLDER/steamlink/apps/moonlight/lib
cp -L "$SDL_INSTALL_ROOT/lib/libSDL3.so.0" "$SDL_INSTALL_ROOT/lib/libSDL3_ttf.so.0" \
  "$DEPLOY_FOLDER/steamlink/apps/moonlight/lib/" || fail "SDL3 library copy failed!"
cp $SOURCE_ROOT/app/deploy/steamlink/* $DEPLOY_FOLDER/steamlink/apps/moonlight/ || fail "Metadata copy failed!"
pushd $DEPLOY_FOLDER
zip -r $INSTALLER_FOLDER/Moonlight-SteamLink-$VERSION.zip . || fail "Zip failed!"
popd

echo Build completed
