#!/bin/sh
# Builds and runs the unit tests of the portable core. No game, no Windows needed.
# Uses AddressSanitizer + UBSan when the compiler can link them, else a plain build (it says which).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
out="${TMPDIR:-/tmp}/multispec_tests"
cxx="${CXX:-g++}"
src="$root/tests/test_spec.cpp $root/src/spec_model.cpp $root/src/swap.cpp"
flags="-std=c++20 -O1 -Wall -Wextra -Werror"
# shellcheck disable=SC2086
if "$cxx" $flags -fsanitize=address,undefined $src -o "$out" 2>/dev/null; then
	echo "build: $cxx with address and undefined-behaviour sanitizers"
else
	"$cxx" $flags $src -o "$out"
	echo "build: $cxx without sanitizers (not available)"
fi
"$out"
