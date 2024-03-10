#!/usr/bin/env bash
# Copyright (c) 2021-2023 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This script is executed inside the builder image

export LC_ALL=C.UTF-8

set -e

source ./ci/dash/matrix.sh

unset CC; unset CXX
unset DISPLAY

mkdir -p $CACHE_DIR/depends
mkdir -p $CACHE_DIR/sdk-sources

ln -s $CACHE_DIR/depends ${DEPENDS_DIR}/built
ln -s $CACHE_DIR/sdk-sources ${DEPENDS_DIR}/sdk-sources

if [[ ${USE_MEMORY_SANITIZER} == "true" ]]; then
  # Use BDB compiled using install_db4.sh script to work around linking issue when using BDB
  # from depends. See https://github.com/bitcoin/bitcoin/pull/18288#discussion_r433189350 for
  # details.
  bash -c "contrib/install_db4.sh \$(pwd) --enable-umrw CC=clang CXX=clang++ CFLAGS='${MSAN_FLAGS}' CXXFLAGS='${MSAN_AND_LIBCXX_FLAGS}'"
fi

mkdir -p ${DEPENDS_DIR}/SDKs

if [ -n "$XCODE_VERSION" ]; then
  OSX_SDK_BASENAME="Xcode-${XCODE_VERSION}-${XCODE_BUILD_ID}-extracted-SDK-with-libcxx-headers.tar.gz"
  OSX_SDK_PATH="${DEPENDS_DIR}/sdk-sources/${OSX_SDK_BASENAME}"
  if [ ! -f "$OSX_SDK_PATH" ]; then
    curl --location --fail "${SDK_URL}/${OSX_SDK_BASENAME}" -o "$OSX_SDK_PATH"
  fi
  if [ -f "$OSX_SDK_PATH" ]; then
    tar -C ${DEPENDS_DIR}/SDKs -xf "$OSX_SDK_PATH"
  fi
fi

make $MAKEJOBS -C depends HOST=$HOST $DEP_OPTS
