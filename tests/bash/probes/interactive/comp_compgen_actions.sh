# compgen actions: functions, aliases, variables, directories and files (V93)
f1() { :; }; f2() { :; }; g1() { :; }
compgen -A function f; echo "rc=$?"
alias ll=ls la=x lz=y
compgen -a l
MYV_ONE=1 MYV_TWO=2
compgen -v MYV_
compgen -a -A function -W "lq fq" l
d=$(mktemp -d)
mkdir "$d/sub1" "$d/sub2"
: >"$d/file1"
compgen -d "$d/su" | sed "s|$d|D|" | sort
compgen -f "$d/" | sed "s|$d|D|" | sort
compgen -d "$d/f"; echo "rc=$?"
rm -r "$d"
