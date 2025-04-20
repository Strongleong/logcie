#!/bin/sh

CFLAGS="-Wall -Wextra -std=c99"
CLIBS="-I. -lmd -lcrypto"
CDEBUG="-ggdb -fsanitize=address -fno-omit-frame-pointer -D_LOGCIE_DEBUG"
CC="gcc"
OUTDIR="./out"
VERBOSE=true
BEAR=false

usage() {
  echo "-d --debug     Compile with debug flags"
  echo "-b --bear      Call bear to generate clang configuration"
  echo "-p --pedantic  Add pedantic flags"
  echo "-s --silent    Compile without unnececary output"
  echo "-o --outdir    Set output dir (default: ./out)"
  echo "-c --compiler  Set which compier to use (default: clang)"
  echo "-h --help      Print help"
}

handle_arg() {
  case "$1" in
    "${2}d"|"--debug") CFLAGS="$CFLAGS $CDEBUG"
     shift
     ;;
    "${2}b"|"--bear") BEAR=true
     shift
     ;;
    "${2}p"|"--pedantic") CFLAGS="$CFLAGS -pedantic -DLOGCIE_PEDANTIC"
     shift
     ;;
    "${2}s"|"--silent") VERBOSE=false
     shift
     shift
     ;;
    "${2}c"|"--compiler") CC="$2"
     shift
     shift
     ;;
    "${2}o"|"--outdir") OUTDIR="$2"
     shift
     shift
     ;;
    "${2}h"|"--help") usage;
      shift
      exit 1;
      ;;
    ""|"-") # pass through
      shift
      ;;
    *) echo "Unknown command '$1'";
      usage;
      exit 1;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -*)
      for (( i=0; i<${#1}; i++ )); do
        handle_arg "${1:$i:1}" ''
      done
      shift
      ;;
    *) handle_arg $1 '-'
      ;;
  esac
done

set -e

# Insuring that flag -c would not undo flag -b
if $BEAR && command -v "bear" > /dev/null 2>&1; then
  CC="bear -- $CC"
else
  echo "WARN: No executable 'bear' was found. Ignoring flag '-b'"
fi

if [ ! -d "$OUTDIR" ]; then
  mkdir -p "$OUTDIR";
fi

if $VERBOSE; then
  set -x
fi

$CC $CFLAGS $CLIBS -o "$OUTDIR/test" ./examples/example.c ./examples/module.c ./examples/libs.c
