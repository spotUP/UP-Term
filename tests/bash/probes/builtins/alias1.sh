shopt -s expand_aliases
alias hi='echo aliased'
hi
unalias hi
type -t hi
