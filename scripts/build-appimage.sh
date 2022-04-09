BUILD_CONFIG="release"

fail()
{
	echo "$1" 1>&2
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
qmake $SOURCE_ROOT/moonlight-qt.pro PREFIX=$DEPLOY_FOLDER/usr DEFINES+=APP_IMAGE || fail "Qmake failed!"
popd

echo Compiling Moonlight in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make -j$(nproc) $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
popd

echo Deploying to staging directory
pushd $BUILD_FOLDER
make install || fail "Make install failed!"
popd

echo Creating AppImage
pushd $INSTALLER_FOLDER
VERSION=$VERSION linuxdeployqt $DEPLOY_FOLDER/usr/share/applications/com.moonlight_stream.Moonlight.desktop -qmldir=$SOURCE_ROOT/app/gui -extra-plugins=platforms/libqeglfs.so,platforms/libqwayland-egl.so,platforms/libqwayland-generic.so,egldeviceintegrations,platformthemes,wayland-decoration-client,wayland-shell-integration,wayland-graphics-integration-client -appimage || fail "linuxdeployqt failed!"
popd

echo Build successful