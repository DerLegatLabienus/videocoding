#!/bin/sh
rm -rf /dev/tpvm
cp /var/log/syslog ./syslog
rmmod tpvm
