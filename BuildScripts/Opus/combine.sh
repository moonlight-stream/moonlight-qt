#!/bin/bash

set -e

source config.sh

for ARCH in $ARCHS
do

  if [ -d dist-$ARCH ]
  then
    MAIN_ARCH=$ARCH
  fi
done

if [ -z "$MAIN_ARCH" ]
then
  echo "Please compile an architecture"
  exit 1
fi


OUTPUT_DIR="dist-uarch"
rm -rf $OUTPUT_DIR

mkdir -p $OUTPUT_DIR/lib $OUTPUT_DIR/include

for LIB in dist-$MAIN_ARCH/lib/*.a
do
  LIB=`basename $LIB`
  LIPO_CREATE=""
  for ARCH in $ARCHS
  do
    LIPO_ARCH=$ARCH
    if [ "$ARCH" = "aarch64" ];
    then
      LIPO_ARCH="arm64"
    fi
    if [ -d dist-$ARCH ]
    then
      LIPO_CREATE="$LIPO_CREATE-arch $LIPO_ARCH dist-$ARCH/lib/$LIB "
    fi
  done
  OUTPUT="$OUTPUT_DIR/lib/$LIB"
  echo "Creating: $OUTPUT"
  xcrun -sdk iphoneos lipo -create $LIPO_CREATE -output $OUTPUT
  xcrun -sdk iphoneos lipo -info $OUTPUT
done

echo "Copying headers from dist-$MAIN_ARCH..."
cp -R dist-$MAIN_ARCH/include/* $OUTPUT_DIR/include
