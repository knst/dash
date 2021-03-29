#!/usr/bin/env bash
#
# Copyright (c) 2018-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

# Make sure default datadir does not exist and is never read by creating a dummy file
if [ "$CI_OS_NAME" == "macos" ]; then
  echo > "${HOME}/Library/Application Support/DashCore"
else
  CI_EXEC echo \> \$HOME/.dashcore
  CI_EXEC_ROOT echo \> \$HOME/.dashcore
fi

CI_EXEC mkdir -p "${DEPENDS_DIR}/SDKs" "${DEPENDS_DIR}/sdk-sources"

if [ -n "$XCODE_VERSION" ] && [ ! -f "$OSX_SDK_PATH" ]; then
  CI_EXEC curl --location --fail "${SDK_URL}/${OSX_SDK_BASENAME}" -o "$OSX_SDK_PATH"
fi

if [ -n "$ANDROID_TOOLS_URL" ]; then
  ANDROID_TOOLS_PATH=$DEPENDS_DIR/sdk-sources/android-tools.zip

  CI_EXEC curl --location --fail "${ANDROID_TOOLS_URL}" -o "$ANDROID_TOOLS_PATH"
  CI_EXEC mkdir -p "${ANDROID_HOME}/cmdline-tools"
  CI_EXEC unzip -o "$ANDROID_TOOLS_PATH" -d "${ANDROID_HOME}/cmdline-tools"
  CI_EXEC "yes | ${ANDROID_HOME}/cmdline-tools/tools/bin/sdkmanager --install \"build-tools;${ANDROID_BUILD_TOOLS_VERSION}\" \"platform-tools\" \"platforms;android-${ANDROID_API_LEVEL}\" \"ndk;${ANDROID_NDK_VERSION}\""
fi

if [ -n "$XCODE_VERSION" ] && [ -f "$OSX_SDK_PATH" ]; then
  CI_EXEC tar -C "${DEPENDS_DIR}/SDKs" -xf "$OSX_SDK_PATH"
fi
if [ -z "$NO_DEPENDS" ]; then
  if [[ $DOCKER_NAME_TAG == *centos* ]]; then
    SHELL_OPTS="CONFIG_SHELL=/bin/dash"
  else
    SHELL_OPTS="CONFIG_SHELL="
  fi
  CI_EXEC "$SHELL_OPTS" make "$MAKEJOBS" -C depends HOST="$HOST" "$DEP_OPTS" LOG=1
fi
if [ "$DOWNLOAD_PREVIOUS_RELEASES" = "true" ]; then
  CI_EXEC test/get_previous_releases.py -b -t "$PREVIOUS_RELEASES_DIR"
fi
