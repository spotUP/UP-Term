a=(x "y z" ""); declare -p a
declare -a e; declare -p e
declare -ar r=(1); declare -p r
declare -ix ai=(1 2); declare -p ai
declare -a g="(1 2)"; declare -p g
a[-1]=q; declare -p a
s='a"b$c`d\e'; t=("$s"); declare -p t
x=1; declare -p x; declare -p nonexist >/dev/null 2>&1; echo $?
declare -A m=([b]=2); declare -p m
declare -A f; declare -p f
declare -p a e | cat
n=(1 2); set | grep -a '^n='
