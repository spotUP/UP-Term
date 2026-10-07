RANDOM=42; echo $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM
RANDOM=1; a=$RANDOM; RANDOM=1; b=$RANDOM; [ "$a" = "$b" ] && echo same
RANDOM=7; for i in 1 2 3 4 5 6 7 8; do printf '%s ' $RANDOM; done; echo
RANDOM=99999; echo $RANDOM
for i in 1 2 3 4 5 6 7 8 9 10; do [ $RANDOM -le 32767 ] || echo big; done; echo ok
