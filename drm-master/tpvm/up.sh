#!/bin/sh

cp /var/log/syslog ./log.bak
> /var/log/syslog

insmod ./tpvm.ko
mknod /dev/tpvm c 250 0

PROCS="/proc/tpvm_viewer_info /proc/tpvm_listener_info /proc/tpvm_metadata /dev/tpvm"

chown `echo $SUDO_USER` $PROCS 
chmod 775               $PROCS
