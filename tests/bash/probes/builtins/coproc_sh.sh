# coproc with sh as the external program (the rig has no sh on its command path: rig-skip.txt)
coproc sh -c 'read x; echo "r:$x"'
echo q >&${COPROC[1]}
read -u ${COPROC[0]} z; echo "$z"
wait; echo "st=$?"
