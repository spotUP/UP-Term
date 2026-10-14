#!/bin/sh
# build_bebbossh.sh -- BebboSSH 1.46.1 built from source: the 1.45 tarball + the 19 patches of dist/net/bebbossh-1.46.1.
# usage: build_bebbossh.sh <drawer> <build dir> [amiga gcc bin dir]
#   <drawer>     dist/net/bebbossh-1.46.1 (the tarball, patches/, SOURCE.txt)
#   <build dir>  build/bebbossh: src/ (unpacked + patched), out/ (the binaries), codesets/ (headers, kept)
# Flags are the source's own (its Makefile, the BUILD section of bebbossh.readme, mkrelease), see make bebbossh.
set -eu
drawer=$(cd "$1" && pwd)
mkdir -p "$2"; b=$(cd "$2" && pwd)
gccbin=${3:-$HOME/opt/amiga/bin}; gccbin=${gccbin%/}
export PATH="$gccbin:$PATH"
CSREPO=https://github.com/jens-maus/libcodesets
CSREV=34442b75bb42666f5d133d74db3350d2159c0a65     # tag 6.22: the headers codesets.library 6.22 is written against

# libcodesets headers (build time only, not shipped): bebbossh.cpp includes libraries/codesets.h, no NDK here has it
if [ ! -f "$b/codesets/include/libraries/codesets.h" ]; then
  rm -rf "$b/codesets"
  git clone -q "$CSREPO" "$b/codesets"
  git -C "$b/codesets" checkout -q "$CSREV"
fi

rm -rf "$b/src" "$b/out"
mkdir -p "$b/src" "$b/out"
tar xjf "$drawer/bebbossh-1.45-src.tar.bz2" -C "$b/src"
cd "$b/src/bebbossh-1.45"
n=0
for p in "$drawer"/patches/0*.patch; do
  # --binary: some sources have CRLF line endings; -F0: no fuzz; a rejected hunk stops the build
  patch -p1 --binary -F0 --no-backup-if-mismatch -s < "$p" || { echo "[ERROR] patch does not apply: $p" >&2; exit 1; }
  n=$((n+1))
done
[ "$n" = 19 ] || { echo "[ERROR] $n patches in $drawer/patches, expected 19" >&2; exit 1; }
echo "[OK] bebbossh: $n of 19 patches applied"

maj=$(sed -n 's/^#define __VERSION_MAJOR "\(.*\)"/\1/p' include/revision.h)
min=$(sed -n 's/^#define __VERSION_MINOR "\(.*\)"/\1/p' include/revision.h)
STRIP="-s -Wl,--gc-sections"
# CC adds hardware/custom.h: src/log.c uses struct Custom and relied on a newer NDK's transitive include
CCX="m68k-amigaos-gcc -include hardware/custom.h"
CXXF="-fno-exceptions -fno-rtti -I$b/codesets/include"
mk() { make -j8 MAJOR="$maj" MINOR="$min" "CC=$CCX" STRIP="$STRIP" Release=1 "C++FLAGS=$CXXF" "$@" >> "$b/build.log" 2>&1; }
: > "$b/build.log"

# programs + library: the 68020 build; then the library variants like mkrelease (make clean between the builds)
mk CPU=-m68020 pre
mk CPU=-m68020 Release/bebbossh Release/bebboscp Release/bebbosshkeygen Release/bebbosshd Release/libcryptossh.library
cp Release/bebbossh Release/bebboscp Release/bebbosshkeygen Release/bebbosshd "$b/out/"
cp Release/libcryptossh.library "$b/out/libcryptossh.library020"
make Release=1 clean >/dev/null 2>&1
mk CPU=-m68060 pre
mk CPU=-m68060 Release/libcryptossh.library
cp Release/libcryptossh.library "$b/out/libcryptossh.library060"
make Release=1 clean >/dev/null 2>&1
mk pre
mk Release/libcryptossh.library
cp Release/libcryptossh.library "$b/out/libcryptossh.library"

# SHA256SUMS of the kit's drawer: the files kept in the repo (everything but our binaries) + our binaries
cd "$drawer"
grep -v -E ' \./(bebbossh|bebboscp|bebbosshd|bebbosshkeygen|libcryptossh\.library(020|060)?)$' SHA256SUMS > "$b/out/SHA256SUMS"
(cd "$b/out" && for f in bebbossh bebboscp bebbosshd bebbosshkeygen libcryptossh.library libcryptossh.library020 libcryptossh.library060; do
   printf '%s  ./%s\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$f"; done) >> "$b/out/SHA256SUMS"
m68k-amigaos-gcc --version | head -1 > "$b/out/TOOLCHAIN.txt"
echo "[OK] bebbossh: built in $b/out"
