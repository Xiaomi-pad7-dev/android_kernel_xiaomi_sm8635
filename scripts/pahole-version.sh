#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Usage: $ ./pahole-version.sh pahole
#
# Prints pahole's version in a 3-digit form, such as 119 for v1.19.

if [ ! -x "$(command -v "$@")" ]; then
	echo 0
	exit 1
fi

output=$("$@" --version 2>/dev/null) || { echo 0; exit 0; }
ver=$(echo "$output" | sed -nE 's/v([0-9]+)\.([0-9]+)/\1\2/p')
if [ -n "$ver" ]; then
	echo "$ver"
else
	echo 0
fi
