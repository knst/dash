#!/usr/bin/env bash
#
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

travis_retry sudo apt update && sudo apt install -y clang-format-9
sudo update-alternatives --install /usr/bin/clang-format      clang-format      $(which clang-format-9     ) 100
sudo update-alternatives --install /usr/bin/clang-format-diff clang-format-diff $(which clang-format-diff-9) 100

travis_retry pip3 install codespell==1.17.1
travis_retry pip3 install flake8==3.8.3
travis_retry pip3 install vulture==2.3
travis_retry pip3 install yq
travis_retry pip3 install mypy==0.700

SHELLCHECK_VERSION=v0.6.0
curl -s "https://storage.googleapis.com/shellcheck/shellcheck-${SHELLCHECK_VERSION}.linux.x86_64.tar.xz" | tar --xz -xf - --directory /tmp/
export PATH="/tmp/shellcheck-${SHELLCHECK_VERSION}:${PATH}"
