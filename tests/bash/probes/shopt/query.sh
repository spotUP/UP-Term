shopt -q nullglob; echo q=$?
shopt -s nullglob; shopt -q nullglob; echo q=$?
shopt nullglob dotglob; echo st=$?
shopt -p nullglob dotglob; echo st=$?
shopt -u nullglob; shopt -p nullglob
shopt -s nosuchoption 2>/dev/null; echo st=$?
shopt -s -u nullglob 2>/dev/null; echo st=$?
shopt -o -s noglob; echo *; shopt -o -u noglob
shopt -s lastpipe; echo hi | read x; echo "[$x]"; shopt -u lastpipe; echo hi | read y; echo "[$y]"
