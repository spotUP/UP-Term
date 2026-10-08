# coproc [NAME] command: NAME[0] reads its output, NAME[1] writes its input, NAME_PID
coproc cat
echo "${#COPROC[@]} $((COPROC_PID > 0))"
w=${COPROC[1]}; r=${COPROC[0]}
echo hello >&$w
read -u $r line; echo "got $line"
echo again >&$w
read -u $r line; echo "got $line"
eval "exec $w>&-"
wait $COPROC_PID; echo "st=$?"
coproc UP { while read l; do echo "<$l>"; done; }
echo abc >&${UP[1]}
read -u ${UP[0]} up; echo "$up"
eval "exec ${UP[1]}>&-"
wait $UP_PID; echo "st=$?"
