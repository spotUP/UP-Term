readonly a=1; echo $a; (a=2) 2>/dev/null; echo rc=$?; readonly b; b=5 2>/dev/null; echo "[$b]"; readonly -p | grep -E "^declare -r (a|b)="
