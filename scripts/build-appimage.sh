BUILD_CONFIG="release"

fail()
{
	echo "$1"
	exit 1
}

BUILD_ROOT=$PWD/build
SOURCE_ROOT=$PWD
BUILD_FOLDER=$BUILD_ROOT/build-$BUILD_CONFIG
DEPLOY_FOLDER=$BUILD_ROOT/deploy-$BUILD_CONFIG
INSTALLER_FOLDER=$BUILD_ROOT/installer-$BUILD_CONFIG

VERSION=$(python3 "$SOURCE_ROOT/scripts/derive-version.py" --source-root "$SOURCE_ROOT" --field artifact)
LINUXDEPLOY=linuxdeploy-$(uname -m).AppImage

command -v qmake6 >/dev/null 2>&1 || fail "Unable to find 'qmake6' in your PATH!"
command -v $LINUXDEPLOY >/dev/null 2>&1 || fail "Unable to find '$LINUXDEPLOY' in your PATH!"

echo "MOONLIGHT BUILD ENVIRONMENT"
qmake --version
echo "QT_ROOT_DIR=$QT_ROOT_DIR"
echo "PATH=$PATH"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"

echo Cleaning output directories
rm -rf $BUILD_FOLDER
rm -rf $DEPLOY_FOLDER
rm -rf $INSTALLER_FOLDER
mkdir $BUILD_ROOT
mkdir $BUILD_FOLDER
mkdir $DEPLOY_FOLDER
mkdir $INSTALLER_FOLDER

# Enable LTO for official builds
export CFLAGS=-flto=auto
export CXXFLAGS=-flto=auto
export LDFLAGS=-flto=auto

echo Configuring the project
pushd $BUILD_FOLDER
# Building with Wayland support will cause linuxdeploy to include libwayland-client.so in the AppImage.
# Since we always use the host implementation of EGL, this can cause libEGL_mesa.so to fail to load due
# to missing symbols from the host's version of libwayland-client.so that aren't present in the older
# version of libwayland-client.so from our AppImage build environment. When this happens, EGL fails to
# work even in X11. To avoid this, we will disable Wayland support for the AppImage.
#
# We disable DRM support because linuxdeploy doesn't bundle the appropriate libraries for Qt EGLFS.
qmake6 $SOURCE_ROOT/moonlight-qt.pro CONFIG+=disable-wayland CONFIG+=disable-libdrm PREFIX=$DEPLOY_FOLDER/usr DEFINES+=APP_IMAGE || fail "Qmake failed!"
popd

echo Compiling Moonlight in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make -j$(nproc) $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
popd

echo Deploying to staging directory
pushd $BUILD_FOLDER
make install || fail "Make install failed!"
popd

echo Updating metadata
perl -pi -e 's/__GITHUB_REF_NAME__/$ENV{GITHUB_REF_NAME}/' $DEPLOY_FOLDER/usr/share/metainfo/com.moonlight_stream.Moonlight.appdata.xml
perl -pi -e 's/__GITHUB_SHA__/$ENV{GITHUB_SHA}/' $DEPLOY_FOLDER/usr/share/metainfo/com.moonlight_stream.Moonlight.appdata.xml

export QML_SOURCES_PATHS=$SOURCE_ROOT/app/gui
export QMAKE=qmake6

echo Creating AppImage
# Remove SQL driver plugins that depend on unavailable system libraries
# (e.g. libqsqlmimer.so → libmimerapi.so) to prevent linuxdeploy/plugin failures
QT_PLUGIN_PATH=$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null)
if [ -n "$QT_PLUGIN_PATH" ] && [ -d "$QT_PLUGIN_PATH/sqldrivers" ]; then
  echo "Removing problematic SQL driver plugins..."
  rm -f "$QT_PLUGIN_PATH/sqldrivers/libqsqlmimer.so"
fi
# Same deal for Qt's GStreamer multimedia backend: linuxdeploy tries to bundle it and
# dies on libgstplay-1.0.so.0 if the build host doesn't have gst-plugins-bad (the
# x86_64 runner image happens to, the aarch64 one doesn't). We only ever use the
# FFmpeg backend — it's Qt's default on Linux since 6.4 and it's what backs the
# QMediaDevices/QAudioSource path in micstream.cpp — so just don't ship the GStreamer one.
if [ -n "$QT_PLUGIN_PATH" ] && [ -d "$QT_PLUGIN_PATH/multimedia" ]; then
  echo "Removing GStreamer multimedia backend..."
  rm -f "$QT_PLUGIN_PATH/multimedia/libgstreamermediaplugin.so"
fi
pushd $INSTALLER_FOLDER
VERSION=$VERSION $LINUXDEPLOY --appdir $DEPLOY_FOLDER \
  --library=/usr/local/lib/libSDL3.so.0 \
  --plugin qt --output appimage || fail "linuxdeploy failed!"
popd

echo Build successful