#!/usr/bin/env sh
set -e

if [ ! -e /source/eml2sql/emlconv ]; then
  echo Build local sources
  cd /source/eml2sql/
  buildDir=/source/eml2sql/build
  if [ -d $buildDir ]; then
    rm -rf /source/eml2sql/build
  else
    mkdir $buildDir
  fi
  if [ -d /source/eml2sql/CMakeFiles ]; then
    rm -rf /source/eml2sql/CMakeFiles
  fi
  cmake -B $buildDir
  cmake --build $buildDir && cd $buildDir && make && cp $buildDir/eml* ..
  cd /data
fi

/source/eml2sql/emlconv /data && sqlite3 /data/eml.sqlite < /source/eml2sql/useful-views

if [ "$1" = "html" ]; then
  cp -f /data/eml.sqlite /source/eml2sql
  cd /source/eml2sql
  ./emlserv
fi
