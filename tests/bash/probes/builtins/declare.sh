declare -i n=5; n=n+2; echo $n; declare -r ro=1; echo $ro; declare -x ex=1; env | grep -c ^ex=
