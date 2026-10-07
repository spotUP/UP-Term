x="a b'c"; n=5; declare -i n; declare -r ro=1; declare -x ex=2; declare -l lo=ABC; declare -u up=abc
a=(1 "b c" 3); declare -A m=([k]=v); declare -n rf=x
echo "Q:${x@Q} ${a[@]@Q} ${a[1]@Q}"
echo "A:${x@A}|${n@A}|${ro@A}|${ex@A}|${lo@A}|${up@A}|${a@A}|${a[@]@A}|${m[@]@A}|${rf@A}|${unsetv@A}|"
echo "a:${x@a}|${n@a}|${ro@a}|${ex@a}|${lo@a}|${up@a}|${a@a}|${a[@]@a}|${m@a}|${rf@a}|"
echo "U:${x@U} ${x@u} ${x@L} ${a[@]@U} ${a[@]@u}"
echo "K:${x@K}|${a[@]@K}|${a[*]@K}|${m[@]@K}|${a[@]@k}"
for w in "${a[@]@k}"; do echo "<$w>"; done
for w in "${a[@]@K}"; do echo "<$w>"; done
y='a\tb\n'; echo "E:${y@E}"
set -- 'p q' r; echo "Q@:${@@Q}|${*@Q}|${@@U}"
z="a\\\\b\\\$ \\\\n"; echo "P2:${z@P}"
v=; echo "Q empty:${v@Q}|${v@A}|${unsetv@Q}|${unsetv@U}|"
