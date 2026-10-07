[ ~+ = "$PWD" ] && echo plus
cd dir; cd ..
[ ~- = "$OLDPWD" ] && echo minus
