# declare +a / +A on an array is refused (status 1) and the array keeps its kind and elements
declare -A m=([k]=v)
declare +A m 2>/dev/null; echo "status $?"
declare -p m
a=(1 2)
declare +a a 2>/dev/null; echo "status $?"
declare -p a
echo "${m[k]} ${a[1]}"
