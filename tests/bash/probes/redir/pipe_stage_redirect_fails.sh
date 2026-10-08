# a pipeline stage whose redirection fails never starts: the next stage gets EOF (it hung: the
# stage's pipe end stayed open in the shell) and the stage's status is 1, not 127 "not found"
wc -c <nonexist_$$ 2>/dev/null | tr -d ' '
echo "st=$? ${PIPESTATUS[@]}"
x=$(cat <nonexist_$$ 2>/dev/null | tr a b); echo "x=[$x] ${PIPESTATUS[@]}"
echo q | cat >nodir_$$/f 2>/dev/null; echo "st=$?"
nosuchcmd_$$ 2>/dev/null | cat; echo "${PIPESTATUS[@]}"
