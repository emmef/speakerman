#!/usr/bin/bash

synopsis() {
  echo -e "USAGE:\n\t$0 [Debug | Release | RelWithDebInfo | MinSizeRel]\n"
}

errorSynopsis() {
  echo "$*" >&2
  synopsis >&2
  popDirectory
  exit 1
}

errorNoSynopsis() {
  echo "$*" >&2
  popDirectory
  exit 1
}
build_type=

case "$1" in
  Debug)
    build_type="$1"
    ;;
  Release)
    build_type="$1"
    ;;
  RelWithDebInfo)
    build_type="$1"
    ;;
  MinSizeRel)
    build_type="$1"
    ;;
  *)
    errorSynopsis "Invalid argument"
    ;;
esac

build_directory="cmake-$build_type"

changeDirectory() {
  local script_directory
  script_directory=$(dirname "$0")
  if ! cd "$script_directory"
  then
    errorNoSynopsis "Could not change to directory where script resides: " "$script_directory"
  fi
}

changeDirectory

cmake -DCMAKE_BUILD_TYPE=$build_type \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang \
  -DCMAKE_DEPENDS_USE_COMPILER=FALSE \
  -G "CodeBlocks - Unix Makefiles" \
  -S ./ \
  -B "./$build_directory"

# echo cmake -DCMAKE_BUILD_TYPE=$build_type \
#  -DCMAKE_C_COMPILER=/usr/bin/clang \
#  -DCMAKE_CXX_COMPILER=/usr/bin/clang \
#  -DCMAKE_DEPENDS_USE_COMPILER=FALSE \
#  -G "CodeBlocks - Unix Makefiles" \
#  -S ./ \
#  -B "./$build_directory"
#

