# This script requires create-dmg to be installed from https://github.com/sindresorhus/create-dmg
BUILD_CONFIG=$1

fail()
{
	echo "$1" 1>&2
	exit 1
}

if [ "$BUILD_CONFIG" != "Debug" ] && [ "$BUILD_CONFIG" != "Release" ]; then
  fail "Invalid build configuration - expected 'Debug' or 'Release'"
fi

[ "$SIGNING_KEY" == "" ] || git diff-index --quiet HEAD -- || fail "Signed release builds must not have unstaged changes!"

BUILD_ROOT=$PWD/build
SOURCE_ROOT=$PWD
BUILD_FOLDER=$BUILD_ROOT/build-$BUILD_CONFIG
INSTALLER_FOLDER=$BUILD_ROOT/installer-$BUILD_CONFIG
VERSION=`cat $SOURCE_ROOT/app/version.txt`

echo Cleaning output directories
rm -rf $BUILD_FOLDER
rm -rf $INSTALLER_FOLDER
mkdir $BUILD_ROOT
mkdir $BUILD_FOLDER
mkdir $INSTALLER_FOLDER

echo Configuring the project
pushd $BUILD_FOLDER
qmake $SOURCE_ROOT/moonlight-qt.pro || fail "Qmake failed!"
popd

echo Compiling Moonlight in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
popd

echo Saving dSYM file
pushd $BUILD_FOLDER
dsymutil app/Moonlight.app/Contents/MacOS/Moonlight -o Moonlight-$VERSION.dsym || fail "dSYM creation failed!"
cp -R Moonlight-$VERSION.dsym $INSTALLER_FOLDER || fail "dSYM copy failed!"
popd

echo Creating app bundle
EXTRA_ARGS=
if [ "$BUILD_CONFIG" == "Debug" ]; then EXTRA_ARGS="$EXTRA_ARGS -use-debug-libs"; fi
echo Extra deployment arguments: $EXTRA_ARGS
macdeployqt $BUILD_FOLDER/app/Moonlight.app $EXTRA_ARGS -qmldir=$SOURCE_ROOT/app/gui -appstore-compliant || fail "macdeployqt failed!"

if [ "$SIGNING_KEY" != "" ]; then
  echo Signing app bundle
  codesign --force --deep --options runtime --timestamp --sign $SIGNING_KEY $BUILD_FOLDER/app/Moonlight.app || fail "Signing failed!"
fi

echo Creating DMG
if [ "$SIGNING_KEY" != "" ]; then
  create-dmg $BUILD_FOLDER/app/Moonlight.app $INSTALLER_FOLDER --identity=$SIGNING_KEY || fail "create-dmg failed!"
else
  create-dmg $BUILD_FOLDER/app/Moonlight.app $INSTALLER_FOLDER
  case $? in
    0) ;;
    2) ;;
    *) fail "create-dmg failed!";;
  esac
fi

if [ "$NOTARY_USERNAME" != "" ] && [ "$NOTARY_PASSWORD" != "" ]; then
  echo Uploading to App Notary service
  xcrun altool -t osx -f $INSTALLER_FOLDER/Moonlight\ $VERSION.dmg --primary-bundle-id com.moonlight-stream.Moonlight --notarize-app --username "$NOTARY_USERNAME" --password "$NOTARY_PASSWORD" --asc-provider $SIGNING_KEY || fail "Notary submission failed"
  
  echo Waiting 5 minutes for notarization to complete
  sleep 300

  echo Getting notarization status
  xcrun altool -t osx --notarization-history 0 --username "$NOTARY_USERNAME" --password "$NOTARY_PASSWORD" --asc-provider $SIGNING_KEY || fail "Unable to fetch notarization history!"

  echo Stapling notary ticket to DMG
  xcrun stapler staple -v $INSTALLER_FOLDER/Moonlight\ $VERSION.dmg || fail "Notary ticket stapling failed!"
fi

mv $INSTALLER_FOLDER/Moonlight\ $VERSION.dmg $INSTALLER_FOLDER/Moonlight-$VERSION.dmg
echo Build successful