#!/bin/sh
# Build nds-basic.nds with the devkitPro docker toolchain.
#   ./build.sh          -> build
#   ./build.sh clean    -> clean
set -e
cd "$(dirname "$0")"
exec docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/src -w /src \
	devkitpro/devkitarm:latest make "$@"
