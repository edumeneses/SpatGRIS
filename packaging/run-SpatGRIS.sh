#!/bin/sh
# Run SpatGRIS straight out of the extracted bundle, without installing.
# SpatGRIS resolves Resources/ against the working directory
# (Source/Misc/sg_DefaultFiles.hpp), so cd into the bundle first.
cd "$(dirname "$(readlink -f "$0")")" || exit 1
exec ./SpatGRIS "$@"
