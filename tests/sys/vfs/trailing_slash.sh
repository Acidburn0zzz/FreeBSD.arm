#!/bin/sh
#
# $FreeBSD: head/tests/sys/vfs/trailing_slash.sh 289224 2015-10-13 16:51:12Z ngie $
#
# Tests vfs_lookup()'s handling of trailing slashes for symlinks that
# point to files.  See kern/21768 for details.  Fixed in r193028.
#

testfile="/tmp/testfile-$$"
testlink="/tmp/testlink-$$"

tests="
$testfile:$testlink:$testfile:0
$testfile:$testlink:$testfile/:1
$testfile:$testlink:$testlink:0
$testfile:$testlink:$testlink/:1
$testfile/:$testlink:$testlink:1
$testfile/:$testlink:$testlink/:1
"

touch $testfile || exit 1
trap "rm $testfile $testlink" EXIT

set $tests
echo "1..$#"
n=1
for testspec ; do
	(
		IFS=:
		set $testspec
		unset IFS
		ln -fs "$1" "$2" || exit 1
		cat "$3" >/dev/null 2>&1
		ret=$?
		if [ "$ret" -eq "$4" ] ; then
			echo "ok $n"
		else
			echo "fail $n - expected $4, got $ret"
		fi
	)
	n=$((n+1))
done
