#!/bin/sh

CFLAGS="-Wall -Wextra -std=c99 -pedantic"
CLIBS="-I./lib/ -lmd -lcrypto"
CDEBUG="-ggdb -fsanitize=address -fno-omit-frame-pointer"
CC="clang"
OUTDIR="./out"
VERBOSE=true

usage() {
  echo "-d --debug     Compile with debug flags"
  echo "-s --silent    Compile without unnececary output"
  echo "-o --outdir    Set output dir (default: ./out)"
  echo "-c --compiler  Set which compier to use (default: clang)"
  echo "-h --help      Print help"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    "-d"|"--debug") CFLAGS="$CFLAGS $CDEBUG"
      shift
      ;;
    "-s"|"--silent") VERBOSE=false
      shift
      ;;
    "-c"|"--compiler") CC="$2"
      shift
      shift
      ;;
    "-o"|"--outdir") OUTDIR="$2"
      shift
      shift
      ;;
    "-h"|"--help") usage;
      shift
      exit 1;
      ;;
    "") # pass through
      ;;
    *) echo "Unknown command '$1'";
      shift
      usage;
      exit 1;
  esac
done

set -e

if [ ! -d "$OUTDIR" ]; then
  mkdir -p "$OUTDIR";
fi

# sources=$(find ./src -name '*.c')

if $VERBOSE; then
  set -x
fi

$CC $CFLAGS $CLIBS -o "$OUTDIR/test" ./test/test.c
