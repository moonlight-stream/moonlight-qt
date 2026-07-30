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

BUILD_ROOT=$PWD/build
SOURCE_ROOT=$PWD
BUILD_FOLDER=$BUILD_ROOT/build-$BUILD_CONFIG
INSTALLER_FOLDER=$BUILD_ROOT/installer-$BUILD_CONFIG

VERSION=$(python3 "$SOURCE_ROOT/scripts/derive-version.py" --source-root "$SOURCE_ROOT" --field artifact)

if [ "$SIGNING_PROVIDER_SHORTNAME" == "" ]; then
  SIGNING_PROVIDER_SHORTNAME=$SIGNING_IDENTITY
fi
if [ "$SIGNING_IDENTITY" == "" ]; then
  SIGNING_IDENTITY=$SIGNING_PROVIDER_SHORTNAME
fi

[ "$SIGNING_IDENTITY" == "" ] || git diff-index --quiet HEAD -- || fail "Signed release builds must not have unstaged changes!"

echo Updating dependencies
python3 $SOURCE_ROOT/setup-deps.py

echo Cleaning output directories
rm -rf $BUILD_FOLDER
rm -rf $INSTALLER_FOLDER
mkdir $BUILD_ROOT
mkdir $BUILD_FOLDER
mkdir $INSTALLER_FOLDER

# Determine target architecture.
# If MOONLIGHT_ARCH is set (e.g. by CI), build only that arch.
# Otherwise detect the native arch to avoid universal binaries,
# which cause multiple macOS TCC permission dialogs due to each
# architecture slice having a different CDHash.
if [ -z "$MOONLIGHT_ARCH" ]; then
  MOONLIGHT_ARCH=$(uname -m)
  # Normalise: Apple Silicon reports arm64, Rosetta reports x86_64
  if [ "$MOONLIGHT_ARCH" = "arm64" ]; then
    MOONLIGHT_ARCH="arm64"
  else
    MOONLIGHT_ARCH="x86_64"
  fi
fi

# Enable LTO for official builds
export CFLAGS=-flto=thin
export CXXFLAGS=-flto=thin
export LDFLAGS=-flto=thin

echo "Configuring the project for architecture: $MOONLIGHT_ARCH"
pushd $BUILD_FOLDER
qmake $SOURCE_ROOT/moonlight-qt.pro QMAKE_APPLE_DEVICE_ARCHS="$MOONLIGHT_ARCH" || fail "Qmake failed!"
popd

echo Compiling Moonlight in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make -j$(sysctl -n hw.logicalcpu) $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
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

# Qt 6.10+ can ship the Mimer SQL driver with an absolute dependency on
# /usr/local/lib/libmimerapi.dylib, which is not present on GitHub's macOS
# runners. Moonlight doesn't use Qt SQL plugins, so remove the problematic
# plugin in CI before macdeployqt scans Qt's plugin tree.
if [ "$GITHUB_ACTIONS" == "true" ]; then
  QT_PLUGIN_DIR=`qmake -query QT_INSTALL_PLUGINS 2>/dev/null`
  if [ -n "$QT_PLUGIN_DIR" ]; then
    rm -f "$QT_PLUGIN_DIR/sqldrivers/libqsqlmimer.dylib"
  fi
fi

echo Copying clipboard helper into app bundle
HELPER_BINARY=$BUILD_FOLDER/clipboard-helper/moonlight-clipboard-helper
if [ ! -f "$HELPER_BINARY" ]; then
  HELPER_BINARY=$BUILD_FOLDER/clipboard-helper/$BUILD_CONFIG/moonlight-clipboard-helper
fi
if [ ! -f "$HELPER_BINARY" ]; then
  HELPER_BINARY=$BUILD_FOLDER/clipboard-helper/$(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]')/moonlight-clipboard-helper
fi
cp "$HELPER_BINARY" $BUILD_FOLDER/app/Moonlight.app/Contents/MacOS/ || fail "Clipboard helper copy failed!"

# macdeployqt only rewrites Qt references in the main executable and the
# plugins it deploys, so the clipboard helper has to be named explicitly with
# -executable. Otherwise it keeps the build machine's absolute Qt paths, which
# either don't exist on the user's machine or pull a second copy of Qt into the
# process alongside the bundled one. Either way the helper aborts at startup
# and clipboard sync silently disables itself.
macdeployqt $BUILD_FOLDER/app/Moonlight.app $EXTRA_ARGS -executable=$BUILD_FOLDER/app/Moonlight.app/Contents/MacOS/moonlight-clipboard-helper -qmldir=$SOURCE_ROOT/app/gui -appstore-compliant || fail "macdeployqt failed!"

echo Building File Provider extension into app bundle
bash "$SOURCE_ROOT/scripts/build-macos-fileprovider-extension.sh" "$SOURCE_ROOT" "$BUILD_FOLDER" "$BUILD_FOLDER/app/Moonlight.app" "$MOONLIGHT_ARCH" || fail "File Provider extension build failed"

echo Removing dSYM files from app bundle
find $BUILD_FOLDER/app/Moonlight.app/ -name '*.dSYM' | xargs rm -rf

if [ "$SIGNING_IDENTITY" != "" ]; then
  echo Signing app bundle
  codesign --force --deep --options runtime --timestamp --sign "$SIGNING_IDENTITY" $BUILD_FOLDER/app/Moonlight.app || fail "Signing failed!"
fi

echo Creating DMG
if [ "$SIGNING_IDENTITY" != "" ]; then
  create-dmg $BUILD_FOLDER/app/Moonlight.app $INSTALLER_FOLDER --identity="$SIGNING_IDENTITY" --no-version-in-filename || fail "create-dmg failed!"
else
  create-dmg $BUILD_FOLDER/app/Moonlight.app $INSTALLER_FOLDER --no-version-in-filename
  CREATE_DMG_STATUS=$?
  case $CREATE_DMG_STATUS in
    0) ;;
    2) ;;
    *)
      echo "create-dmg failed with status $CREATE_DMG_STATUS; falling back to hdiutil"
      rm -f $INSTALLER_FOLDER/Moonlight.dmg
      hdiutil create -volname Moonlight -srcfolder $BUILD_FOLDER/app/Moonlight.app -ov -format UDZO $INSTALLER_FOLDER/Moonlight.dmg || fail "fallback hdiutil DMG creation failed!"
      ;;
  esac
fi

if [ "$NOTARY_KEYCHAIN_PROFILE" != "" ]; then
  echo Uploading to App Notary service
  xcrun notarytool submit --keychain-profile "$NOTARY_KEYCHAIN_PROFILE" --wait $INSTALLER_FOLDER/Moonlight.dmg || fail "Notary submission failed"

  echo Stapling notary ticket to DMG
  xcrun stapler staple -v $INSTALLER_FOLDER/Moonlight.dmg || fail "Notary ticket stapling failed!"
fi

mv $INSTALLER_FOLDER/Moonlight.dmg $INSTALLER_FOLDER/Moonlight-$VERSION.dmg
echo Build successful
