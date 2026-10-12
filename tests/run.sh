#!/bin/sh
# run.sh -- translate, build and run every tests/t*.cob, compare with
# the expected output in tests/NAME.out (stdin from tests/NAME.in)
cd "`dirname "$0"`/.." || exit 1
top=`pwd`
COBOL=${COBOL:-./cobol}
pass=0; fail=0
mkdir -p /tmp/cobtest
for src in tests/t*.cob; do
  n=`basename $src .cob`
  [ -n "$1" ] && [ "$1" != "$n" ] && continue
  exe=/tmp/cobtest/$n
  extra=""
  [ -f tests/$n.opts ] && extra=`cat tests/$n.opts`
  if [ -f tests/$n.pre ]; then
    COBOL_ABS=$top/$COBOL TOP=$top export COBOL_ABS TOP
    sh tests/$n.pre
  fi
  cc=""
  [ -n "$COBOL_CC" ] && cc="-cc $COBOL_CC"
  if ! $COBOL -c $cc $src $extra -o $exe > /tmp/cobtest/$n.log 2>&1
  then
    echo "FAIL $n (translate/build)"; head -5 /tmp/cobtest/$n.log
    fail=`expr $fail + 1`; continue
  fi
  inp=/dev/null
  [ -f tests/$n.in ] && inp=$top/tests/$n.in
  (cd /tmp/cobtest && COB_LIBRARY_PATH=/tmp/cobtest timeout 20 $exe < $inp > $n.act 2>&1)
  if cmp -s /tmp/cobtest/$n.act tests/$n.out; then
    pass=`expr $pass + 1`
  else
    echo "FAIL $n"
    diff /tmp/cobtest/$n.act tests/$n.out | head -10
    fail=`expr $fail + 1`
  fi
done
echo "passed $pass, failed $fail"
[ $fail = 0 ]
