trap 'echo "DBG $BASH_COMMAND"' DEBUG
[[ a == a && b == b ]]
[[ a == a || -n $HOME ]]
[[ ! -z x ]]
[[ ( a == a ) && b != c ]]
[[ a =~ ^a ]]
[[ -f /nonexist || a < b ]]
[[ a == a &&
   b == b ]]
for (( i=0; i<2; i++ )); do echo $i; done
for ((;;)); do break; done
for (( i = 0 ; i < 1 ; i++ ))
do
 echo x
done
x=$(( 1+2 ))
(( x )) && echo y
[[ -n "a b" ]]
[[ $x == "a"* ]]
