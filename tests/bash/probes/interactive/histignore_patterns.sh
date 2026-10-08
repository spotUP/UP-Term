HISTIGNORE='ls:ls *:true'
echo a
ls / >/dev/null
ls -l / >/dev/null
true
echo b
history
