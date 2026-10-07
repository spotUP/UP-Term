pr() { local s o=; for s in "${BASH_SOURCE[@]}"; do o="$o ${s##*/}"; done; echo "$1: ${FUNCNAME[*]}|$o|${BASH_LINENO[*]}"; }
pr top
g() { pr g; }
f() {
  g
}
f
g
src=${TMPDIR:-/tmp}/vsh_src_probe_bs.sh
printf '%s\n' 'pr src' 'h() { pr h; }' > "$src"
source "$src"
h
k() { source "$src"; h; }
k
rm -f "$src"
eval 'pr ev'
