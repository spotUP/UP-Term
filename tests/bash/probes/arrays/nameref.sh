a=(1 2 3); declare -n r=a; echo ${r[1]} ${#r[@]} "${r[@]}"; r[1]=x; echo ${a[@]}; r=(7 8); echo ${a[@]}
v=val; declare -n s=v; echo $s ${!s}; s=new; echo $v; unset -n s; echo "${s-gone}" $v
declare -n t=v; unset t; echo "${v-gone}"
f() { local -n lr=$1; lr=changed; }; w=orig; f w; echo $w
declare -n c1=c2 c2=c1 2>/dev/null; echo ok
x=1; y=x; echo ${!y}
declare -n self=a; declare -p self
g() { local -n ar=$1; ar+=(z); }; arr=(1); g arr; echo ${arr[@]}
