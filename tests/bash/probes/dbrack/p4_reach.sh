n=0
for (( i = 0; i < 4; i++ )); do (( n += i )); done
if [[ $n -eq 6 && "v$n.0" =~ ^v([0-9]+)\.([0-9])$ ]]; then echo "n=$n m=${BASH_REMATCH[1]} z=${BASH_REMATCH[2]}"; fi
echo $(( n > 5 ? n * 2 : 0 )) $(( n++ )) $n
