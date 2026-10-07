shopt -s expand_aliases
alias cat='echo ALIAS'
alias hi='echo aliased'
X=cat
$X /dev/null
"cat" /dev/null
'cat' /dev/null
\cat /dev/null
c"at" /dev/null
$(echo hi)
`echo hi`
${X/cat/hi}
cat
hi
command hi 2>/dev/null
eval hi
eval 'cat'
(hi)
{ hi; }
if hi; then hi; fi
