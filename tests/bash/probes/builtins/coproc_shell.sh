# coproc with a shell body, and line reads from a pipe whose writer is still running: each line
# reaches the reader when it is written, not when the writer ends (Amiga: PIPE: fills a buffered
# read only when the buffer is full or the writer closes, so these hung; the rig runs this probe)
coproc UP { while read l; do echo "<$l>"; done; }
echo abc >&${UP[1]}
read -u ${UP[0]} up; echo "$up"
echo def >&${UP[1]}
read -u ${UP[0]} up; echo "$up"
eval "exec ${UP[1]}>&-"
wait $UP_PID; echo "st=$?"
coproc { read x; echo "r:$x"; read y; echo "r:$y"; }
echo one >&${COPROC[1]}
read -u ${COPROC[0]} z; echo "$z"
echo two >&${COPROC[1]}
read -u ${COPROC[0]} z; echo "$z"
wait; echo "st=$?"
# the rest of the pipe stays for the next reader
printf 'a\nb\nc\n' | { read x; echo "x=$x"; cat; }
