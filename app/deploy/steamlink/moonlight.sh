#!/bin/sh

# The default HOME is not persistent, so override
# it to a path on the onboard flash. Otherwise our
# pairing data will be lost each reboot.
HOME=/usr/local/moonlight

# For a seamless transition across the Qt 5.9 -> Qt 5.14 boundary,
# we bundle binaries built with both Qt 5.9 and Qt 5.14 and pick
# the correct one at runtime.
if [ -d "/usr/local/Qt-5.14.1" ]; then
    BIN="moonlight"
else
    BIN="moonlight59"
fi

# Renice PE_Single_CPU which seems to host A/V stuff
renice -10 -p $(pidof PE_Single_CPU)

# Renice Moonlight itself to avoid preemption by background tasks
# Write output to a logfile in /tmp
echo Launching $BIN
exec nice -n -10 ./bin/$BIN > /tmp/moonlight.log
