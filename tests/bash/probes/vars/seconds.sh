echo $SECONDS
SECONDS=100; [ "$SECONDS" -ge 100 ] && [ "$SECONDS" -le 101 ] && echo reset
t=$EPOCHSECONDS; [ "$t" -gt 1700000000 ] && echo epoch
case $EPOCHREALTIME in [0-9]*.[0-9][0-9][0-9][0-9][0-9][0-9]) echo real;; esac
