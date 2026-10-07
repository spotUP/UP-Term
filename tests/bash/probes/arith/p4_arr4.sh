declare -A m; m[x]=5; k=x; echo $((m[x]+1)) $((m[$k]*2))
