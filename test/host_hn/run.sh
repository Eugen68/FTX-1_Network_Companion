#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
node "$here/test_hn.cjs"
