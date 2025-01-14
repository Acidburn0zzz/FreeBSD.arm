#!/bin/sh
# $FreeBSD: head/tests/sys/geom/class/eli/init_i_P_test.sh 293821 2016-01-13 09:14:27Z ngie $

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
keyfile=`mktemp $base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..1"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

geli init -B none -i 64 -P -K ${keyfile} md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

rm -f $keyfile
