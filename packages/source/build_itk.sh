#!/usr/bin/env bash

set -e

[ "$#" != "1" ] && echo "error: usage: build.sh <prefix>" && exit 1
INSTALL_DIR="$1"

export CC=$(which gcc)
export CXX=$(which g++)

mkdir build
cd build

cmake ../ITK-5.0a01 \
  -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
  -DITKGroup_Core=ON \
  -DITKGroup_Filtering=ON \
  -DITKGroup_Segmentation=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCMAKE_CXX_FLAGS="-msse2 -mfpmath=sse" \
  -DCMAKE_C_FLAGS="-msse2 -mfpmath=sse" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10

make -j8
make install
