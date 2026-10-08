# DIRSTACK[n]=dir sets the n-th directory of the stack; element 0 and indexes past the stack are dropped
mkdir -p a b c
d() { dirs -p | while IFS= read -r l; do echo "${l##*/}"; done; }
b() { local p; p=$(pwd); echo "${p##*/}"; }
pushd a >/dev/null; pushd b >/dev/null
echo "${#DIRSTACK[@]}"
DIRSTACK[1]=/x; d
DIRSTACK[2]=/y; d
DIRSTACK[5]=/z; d
DIRSTACK[0]=/q; d; b
DIRSTACK=(/p /m /n); d
popd >/dev/null; b
unset DIRSTACK
DIRSTACK[1]=/after; d
e=(); for x in "${DIRSTACK[@]}"; do e+=("${x##*/}"); done; echo "${e[@]}"
