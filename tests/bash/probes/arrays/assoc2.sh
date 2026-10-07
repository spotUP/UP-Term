declare -A m; m[a]=1; m[b c]=2; m["d"]=3; k=e; m[$k]=4
for x in "${!m[@]}"; do echo "$x=${m[$x]}"; done | sort
echo ${#m[@]} "${m[b c]}" ${m[zz]-none}
declare -A n=([x]=1 [y]=2); n[x]+=a; echo ${n[x]} ${n[y]}
unset 'n[x]'; echo ${!n[@]}
declare -A o=(k1 v1 k2 v2); echo ${o[k1]} ${o[k2]}
echo ${m[a]:-d} ${m[nope]:-d}
declare -A p; p[0]=zero; echo $p ${p[0]}
m=([only]=1); echo ${!m[@]}
for v in "${m[@]}"; do echo $v; done
declare -p m | cat
declare -A q=([a b]=1); declare -p q
declare -a bad; declare -A bad 2>/dev/null; echo $?
