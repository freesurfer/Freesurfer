#!/usr/bin/env bash

#
# script to run the travis build step
#

function runstep {
  $1 >> build.log 2>&1 || { echo $1; tail -n 100 build.log; exit 1; }
}

runstep ./setup_configure

config_flags="--with-pkgs-dir=${PWD}/build-packages --disable-Werror --disable-GUI-build"
[[ "$TRAVIS_OS_NAME" == "osx" ]] && config_flags="${configure_flags} F77=/usr/local/bin/gfortran-4.9 CC=/usr/local/bin/gcc-4.9 CXX=/usr/local/bin/g++-4.9"

runstep ./configure ${config_flags}
runstep make -j4
