#!/bin/bash

set -e
set -o errexit -o pipefail -o noclobber -o nounset

CFLAGS="-Wall -Wextra -std=c99"
CPPFLAGS="-Wall -Wextra"
CLIBS="-I."
CDEBUG="-ggdb -fsanitize=address -fno-omit-frame-pointer -D_LOGCIE_DEBUG -O0"
CC="gcc"
CPP="c++"
OUTDIR="./out"
VERBOSE=true
BEAR=false
COMPILE=true

usage() {
  echo "-d --debug     Compile with debug flags"
  echo "-b --bear      Call bear to generate clang configuration"
  echo "-p --pedantic  Add pedantic flags"
  echo "-s --silent    Compile without unnececary output"
  echo "-o --outdir    Set output dir (default: ./out)"
  echo "-c --compiler  Set which compier to use (default: clang)"
  echo "-r --dry-run   Do not compile"
  echo "-h --help      Print help"
}

getopt --test > /dev/null && true
if [[ $? -eq 4 ]]; then
  LONGOPTS=dry-run,debug,bear,pedantic,silent,outdir:,compiler:,help
  OPTIONS=tdbpso:c:h
  PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@") || exit 2
  eval set -- "$PARSED"
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    "-d"|"--debug") CFLAGS="$CFLAGS $CDEBUG"
      shift
      ;;
    "-b"|"--bear") BEAR=true
      shift
      ;;
    "-p"|"--pedantic") CFLAGS="$CFLAGS -pedantic -DLOGCIE_PEDANTIC"
      shift
      ;;
    "-s"|"--silent") VERBOSE=false
      shift
      ;;
    "-r"|"--dry-run") COMPILE=false
      shift
      ;;
    "-c"|"--compiler") CC="$2"
      shift 2
      ;;
    "-o"|"--outdir") OUTDIR="$2"
      shift 2
      ;;
    "-h"|"--help") usage;
      shift
      exit 1;
      ;;
    ""|"-") # pass through
      shift
      ;;
    "--")
      shift
      break
      ;;
    *) echo "Unknown command '$1'";
      usage;
      exit 1;
  esac
done

# Insuring that flag -c would not undo flag -b
if $BEAR; then
  if command -v "bear" > /dev/null 2>&1; then
    CC="bear -- $CC"
  else
    echo "WARN: No executable 'bear' was found. Ignoring flag '-b'"
  fi
fi

if [ ! -d "$OUTDIR" ]; then
  mkdir -p "$OUTDIR";
fi

c_sources=$(find . -name '*.c')
cpp_sources=$(find . -name '*.cpp')

for source in $c_sources; do
  CMD="$CC $CFLAGS $CLIBS -o $OUTDIR/$(basename ${source%.*}) $source"

  if $VERBOSE; then
    echo $CMD
  fi

  if $COMPILE; then
    $CMD &
  fi
done

for source in $cpp_sources; do
  CMD="$CPP $CPPFLAGS $CLIBS -o $OUTDIR/$(basename ${source%.*}) $source"

  if $VERBOSE; then
    echo $CMD
  fi

  if $COMPILE; then
    $CMD &
  fi
done

status=0

for job in $(jobs -p); do
  wait $job || status=1
done

exit $status
