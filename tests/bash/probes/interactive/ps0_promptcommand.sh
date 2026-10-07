PROMPT_COMMAND='echo pc-$?'
PS0="ps0\n"
echo x
false
echo $?
PROMPT_COMMAND=(echo A echo B)
true
