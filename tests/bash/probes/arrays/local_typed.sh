a=(1 2 3); declare -i n=5; export X=out
f() { local -a a=(x); local -i n=7; local X=in; echo "${a[@]} $n $X"; a+=(y); echo "${a[@]}"; g; }
g() { echo "g: ${a[@]} $n $X"; }
f; echo "${a[@]} $n $X"; declare -p n X
h() { local a; a=str; echo "$a"; declare -p a; unset a; echo "${a-unset}"; }; h; echo "${a[@]}"
k() { local -A m=([x]=1); m[y]=2; echo ${#m[@]}; }; k; echo "${m[@]-gone}"
l() { declare -g gl=(1 2); declare lc=(3); echo "${lc[@]}"; }; l; echo "${gl[@]}" "${lc-nolc}"
u() { local arr=(1 2); unset arr; echo "${arr-gone}"; }; arr=(9); u; echo "${arr[@]}"
w() { local v=1; ( echo $v ); v=2; echo $v; }; v=0; w; echo $v
p() { local -r ro=1; echo $ro; }; p
Y=(a b); Y2() { Y=(c); }; Y2; echo ${Y[@]}
t() { local tt; tt=(1 2); echo ${#tt[@]}; }; t; echo ${tt-no}
