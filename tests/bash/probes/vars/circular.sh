declare -n c1=c2; declare -n c2=c1; echo "[$c1]"
declare -n a1=a2 a2=a3 a3=a4 a4=a5 a5=a6 a6=a7 a7=a8 a8=a9 a9=a10 a10=val
val=deep; echo "[$a1] [$a9]"
declare -n s=s; echo "[$s]"
f() { local -n x=x; echo "[$x]"; }; f
declare -n n1=n2; declare -n n2=n3; n3=end; echo "$n1"
