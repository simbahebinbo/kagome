#!/bin/bash -xe
#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

BUILD_DIR=build

BUILD_TYPE="${BUILD_TYPE:?BUILD_TYPE variable is not defined}"

if [ "$BUILD_TYPE" != "Debug" ] && [ "$BUILD_TYPE" != "Release" ] && [ "$BUILD_TYPE" != "RelWithDebInfo" ]; then
  echo "Invalid build type $BUILD_TYPE, should be either Debug, Release or RelWithDebInfo"
fi

if [[ -z "${CI}" ]]; then

if [ "$BUILD_TYPE" = "Release" ]; then
  BUILD_THREADS=1
elif [ "$BUILD_TYPE" = "RelWithDebInfo" ]; then
  BUILD_THREADS=2
else
  BUILD_THREADS="${BUILD_THREADS:-$(( $(nproc 2>/dev/null || sysctl -n hw.ncpu) / 2 + 1 ))}"
fi

else # CI
  BUILD_THREADS="${BUILD_THREADS:-$(( $(nproc 2>/dev/null || sysctl -n hw.ncpu) ))}"
  # Configure CI git security
  git config --global --add safe.directory /__w/kagome/kagome
fi

if [[ "${KAGOME_IN_DOCKER}" = 1 ]]; then
  source /venv/bin/activate
fi

git submodule update --init

cd "$(dirname $0)/../../.."
echo "Building in $(pwd)"

cmake . -B"${BUILD_DIR}" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DBACKWARD=OFF
cmake --build "${BUILD_DIR}" --target kagome -- -j${BUILD_THREADS}
