#!/bin/bash

CONF=/etc/sysctl.d/10-coredump.conf

pvoip3_board ()
{
	[ -r /proc/umjs/reg ] || return 1
	grep -sq BBD=4 /proc/umjs/reg && return 0
	return 1
}

dump_enable ()
{
	local SRC=/usr/share/doc/coredump/examples/coredump.default.conf
	pvoip3_board && SRC=/usr/share/doc/coredump/examples/coredump.pvoip3.conf
	cp -fv $SRC $CONF
}

case $1 in
  enable)
	dump_enable
	;;
  disable)
	echo -n > $CONF
	;;
  *)
	echo "Usage: $0 {enable|disable}"
	exit 0
	;;
esac

sysctl -p $CONF

exit 0
