#!/bin/sh
# package.sh VERSION -- build release packages into dist/
# Linux binary is linked statically so it runs on any x86_64 distro.
set -e
v=${1:-1.0}
root=`pwd`
d=$root/dist
mkdir -p $d
cc -O2 -std=c89 -o $d/cobol-linux-x86_64 cobol_all.c -lm -static
# Windows zips: cobol.exe, runtime header, manual, examples
for a in i686:win32 x86_64:win64; do
  arch=${a%%:*}; tag=${a##*:}
  w=$d/cobol-$v-$tag
  rm -rf $w; mkdir -p $w
  cp $d/cobol-$arch.exe $w/cobol.exe
  cp src/cobrt.h README.md $w/
  [ -d $d/tcc-$tag ] && cp -r $d/tcc-$tag $w/tcc
  cp -r examples $w/
  G=docs/COBOL-User-Guide.docx; [ -f $G ] && cp $G $w/
  (cd $d && rm -f cobol-$v-$tag.zip &&
   zip -qr cobol-$v-$tag.zip cobol-$v-$tag)
  rm -rf $w
done
# Linux tarball with binary
t=$d/cobol-$v-linux-x86_64
rm -rf $t; mkdir -p $t/bin $t/share/cobol
cp $d/cobol-linux-x86_64 $t/bin/cobol
cp src/cobrt.h $t/share/cobol/
cp -r examples $t/share/cobol/
cp README.md $t/
(cd $d && tar czf cobol-$v-linux-x86_64.tar.gz cobol-$v-linux-x86_64)
rm -rf $t
# source tarball and rpm
s=$d/src/cobol-$v
rm -rf $d/src; mkdir -p $s
cp -r Makefile README.md cobol.spec cobol_all.c src tools \
   tests examples $s/
(cd $d/src && tar czf $d/cobol-$v-src.tar.gz cobol-$v)
if which rpmbuild >/dev/null 2>&1; then
  r=$d/rpmbuild; rm -rf $r; mkdir -p $r/SOURCES $r/SPECS
  cp $d/cobol-$v-src.tar.gz $r/SOURCES/cobol-$v.tar.gz
  cp cobol.spec $r/SPECS/
  rpmbuild -bb --nodeps --define "_topdir $r" \
    --define "debug_package %{nil}" $r/SPECS/cobol.spec >/dev/null
  cp $r/RPMS/*/*.rpm $d/
fi
# deb
if which dpkg-deb >/dev/null 2>&1; then
  b=$d/deb/cobol_${v}_amd64
  rm -rf $d/deb; mkdir -p $b/DEBIAN $b/usr/bin $b/usr/share/cobol
  cp $d/cobol-linux-x86_64 $b/usr/bin/cobol
  cp src/cobrt.h $b/usr/share/cobol/
  cp -r examples $b/usr/share/cobol/
  cat > $b/DEBIAN/control <<EOT
Package: cobol
Version: $v
Architecture: amd64
Maintainer: cobol project
Description: COBOL to C89 translator
 Translates COBOL to portable C89; needs a C compiler.
EOT
  dpkg-deb --build $b $d/cobol_${v}_amd64.deb >/dev/null
fi
rm -rf $d/src $d/rpmbuild $d/deb
ls -l $d
