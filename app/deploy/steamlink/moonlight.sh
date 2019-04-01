#!/bin/sh

# The default HOME is not persistent, so override
# it to a path on the onboard flash. Otherwise our
# pairing data will be lost each reboot.
HOME=/usr/local/moonlight

# Write output to a logfile in /tmp
exec ./bin/moonlight > /tmp/moonlight.log
