#!/bin/sh
# mkrt.sh -- assemble src/cobrt.h from the rt*.h parts
# usage: sh tools/mkrt.sh > src/cobrt.h
cd "`dirname "$0"`/.." || exit 1
cat src/rt0_head.h
echo
echo "/* intrinsic function numbers */"
awk '/^static const char \*intrinsics\[\]/ {on=1; next}
     on && /^};/ {on=0; next}
     on { n = split($0, w, "\"");
          for (i = 2; i <= n; i += 2) { s = w[i]; gsub("-", "_", s);
              printf("#define FN_%s %d\n", s, ++k) } }' src/c40_expr.c
echo
for f in src/rt00_const.h src/rt1_dec.h src/rt2_fld.h src/rt3_core.h \
         src/rt4_misc.h \
         src/rt5_str.h src/rt6_fn.h src/rt7_file.h
do
  if [ -f "$f" ]; then
    echo
    cat "$f"
  fi
done
echo
echo "#endif"
