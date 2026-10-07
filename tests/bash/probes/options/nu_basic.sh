set -u; echo ${x-d} ${x:-d} ${x+p} ${x:+p}; echo "$@"; echo $*; echo ${#@}; echo ok
