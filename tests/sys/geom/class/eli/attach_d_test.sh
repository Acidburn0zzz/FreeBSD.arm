#!/bin/sh
# $FreeBSD: head/tests/sys/geom/class/eli/attach_d_test.sh 293821 2016-01-13 09:14:27Z ngie $

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
keyfile=`mktemp $base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..3"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

geli init -B none -P -K $keyfile md${no}
geli attach -d -p -k $keyfile md${no}
if [ -c /dev/md${no}.eli ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
# Be sure it doesn't detach on read.
dd if=/dev/md${no}.eli of=/dev/null 2>/dev/null
sleep 1
if [ -c /dev/md${no}.eli ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi
true > /dev/md${no}.eli
sleep 1
if [ ! -c /dev/md${no}.eli ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

rm -f $keyfile
