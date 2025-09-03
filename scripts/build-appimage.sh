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
VERSION=`cat $SOURCE_ROOT/app/version.txt`

command -v qmake >/dev/null 2>&1 || fail "Unable to find 'qmake' in your PATH!"
command -v linuxdeployqt >/dev/null 2>&1 || fail "Unable to find 'linuxdeployqt' in your PATH!"

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

echo Configuring the project
pushd $BUILD_FOLDER
# Building with Wayland support will cause linuxdeployqt to include libwayland-client.so in the AppImage.
# Since we always use the host implementation of EGL, this can cause libEGL_mesa.so to fail to load due
# to missing symbols from the host's version of libwayland-client.so that aren't present in the older
# version of libwayland-client.so from our AppImage build environment. When this happens, EGL fails to
# work even in X11. To avoid this, we will disable Wayland support for the AppImage.
#
# We disable DRM support because linuxdeployqt doesn't bundle the appropriate libraries for Qt EGLFS.
qmake $SOURCE_ROOT/moonlight-qt.pro CONFIG+=disable-wayland CONFIG+=disable-libdrm CONFIG+=disable-cuda PREFIX=$DEPLOY_FOLDER/usr DEFINES+=APP_IMAGE || fail "Qmake failed!"
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

echo Creating AppImage
pushd $INSTALLER_FOLDER
VERSION=$VERSION \
  linuxdeployqt \
  $DEPLOY_FOLDER/usr/share/applications/com.moonlight_stream.Moonlight.desktop \
  -qmldir=$SOURCE_ROOT/app/gui \
  -appimage \
  -exclude-libs=libqsqlmimer || fail "linuxdeployqt failed!"
popd

echo Build successful
