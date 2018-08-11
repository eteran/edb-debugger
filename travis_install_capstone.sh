#!/bin/sh

set -ex
mkdir -p $HOME/src
cd $HOME/src
git clone --depth=50 --branch=3.0.5 https://github.com/aquynh/capstone.git
mkdir -p $HOME/src/capstone/build
cd $HOME/src/capstone/build
cmake ..
make
sudo make install
cd $TRAVIS_BUILD_DIR
