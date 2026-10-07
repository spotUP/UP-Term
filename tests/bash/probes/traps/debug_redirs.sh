exec 3>&1
trap 'echo "DBG $BASH_COMMAND" >&3' DEBUG
true >f1
true > f2
true >>f3
true <f1
true 2>&1
true 2>f4
true 3>&-
true <&0
true >&2
true >| f5
true <> f6
true &> f7
true &>> f8
true {v}>f9
true <<< hi
true 1>f1
true 0<f1
true 3>f1
true 3<f1
true <&-
true 2>&-
true 4>&1
true 7<&3
true "a b" 'c d' $x ${y:-z} >"o p"
x=1 y=2 true >f1
x=1 y=2
>f1
true 4<f1 5>&4 6<&-  >&-
