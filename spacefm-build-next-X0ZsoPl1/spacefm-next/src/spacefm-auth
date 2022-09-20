#!/bin/bash
# This script (spacefm-auth) is used internally by spacefm to authenticate
# temporary scripts run as another user.  This file should not be modified
# or run directly.

if [ "$1" = "root" ]; then
	shift
	fm_root=1
else
	fm_root=0
fi

if [ "$2" = "" ]; then
	echo "spacefm-auth: This script is used internally by spacefm and is not" 1>&2
	echo "              designed to be run directly." 1>&2
	exit 1
fi
	
fm_source="$1"
fm_sum="$2"

if [ ! -f "$fm_source" ]; then
	echo "spacefm-auth: error: missing file"
	exit 1
fi

if [ ${#fm_sum} -eq 64 ]; then
	fm_sha=/usr/bin/sha256sum
elif [ ${#fm_sum} -eq 128 ]; then
	fm_sha=/usr/bin/sha512sum
else
	echo "spacefm-auth: error: invalid sum" 1>&2
	exit 1
fi

if (( fm_root == 1 )); then
	chown root:root "$(dirname "$fm_source")" || exit 1
	chmod ugo+rwx,+t "$(dirname "$fm_source")" || exit 1
	chmod go-rwx "$fm_source" || exit 1
fi

if [ "$($fm_sha "$fm_source")" != "$fm_sum  $fm_source" ]; then
	echo "spacefm-auth: error: $fm_sha mismatch" 1>&2
	exit 1
fi

export fm_sum
/bin/bash "$fm_source" run
exit $?

