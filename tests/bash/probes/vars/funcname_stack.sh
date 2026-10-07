echo "top: [${FUNCNAME[@]}] [${FUNCNAME-unset}] ${#FUNCNAME[@]}"
g() { echo "g: ${FUNCNAME[@]} | ${FUNCNAME[0]} ${FUNCNAME[1]} ${FUNCNAME[2]} | ${#FUNCNAME[@]}"; }
f() { g; }
f
f2() { echo "${FUNCNAME[@]}"; ( echo "sub ${FUNCNAME[@]}" ); echo "$(echo ${FUNCNAME[0]})"; }
f2
FUNCNAME=5; echo "[$FUNCNAME]"
r() { if [ "$1" -gt 0 ]; then r $(($1 - 1)); else echo ${#FUNCNAME[@]} "${FUNCNAME[@]}"; fi; }
r 3
