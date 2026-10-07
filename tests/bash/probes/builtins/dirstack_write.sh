# DIRSTACK[n]=dir sets the n-th directory of the stack; element 0 and indexes past the stack are dropped
mkdir -p a b c
d() { dirs -p | sed 's|.*/||'; }
pushd a >/dev/null; pushd b >/dev/null
echo "${#DIRSTACK[@]}"
DIRSTACK[1]=/x; d
DIRSTACK[2]=/y; d
DIRSTACK[5]=/z; d
DIRSTACK[0]=/q; d; pwd | sed 's|.*/||'
DIRSTACK=(/p /m /n); d
popd >/dev/null; pwd | sed 's|.*/||'
unset DIRSTACK
DIRSTACK[1]=/after; d
echo "${DIRSTACK[@]}" | sed 's|/[^ ]*/||g'
