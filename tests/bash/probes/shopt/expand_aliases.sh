alias x='echo hi'
x
shopt expand_aliases
shopt -s expand_aliases
alias y='echo yo'
y
type y
shopt -u expand_aliases
y
shopt -q expand_aliases; echo $?
