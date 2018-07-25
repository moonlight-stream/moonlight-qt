#!/bin/sh

# The ImageMagick conversion tool doesn't seem to always generate
# ICO files with background transparency properly. Please validate
# that the output has a transparent background.

convert -density 256 -background none -define icon:auto-resize ../app/res/moonlight.svg ../app/moonlight.ico

echo IMPORTANT: Validate the icon has a transparent background before committing!
