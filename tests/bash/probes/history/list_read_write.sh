T=$(mktemp -d)
printf 'one\ntwo words\nthree\n' > $T/h
HISTFILE=$T/h
history
echo "-- r"; history -r; history
echo "-- n"; history 2; history 1; history 0; history 99
echo "-- s"; history -s "a   b" c; history 1
echo "-- w"; HISTFILE=$T/o; history -w; cat $T/o
echo "-- size"; HISTFILESIZE=2; history -w; cat $T/o; unset HISTFILESIZE
echo "-- p"; history -p x y; echo $?
echo "-- c"; history -c; history; echo st=$?
rm -rf $T
