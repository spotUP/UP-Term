PS3='pick> '
select x in apple banana cherry; do echo "got [$x] reply [$REPLY]"; [ "$REPLY" = 3 ] && break; done <<EOT
2
9

x
1
3
EOT
echo "after st=$?"
select y in a b; do echo y; done </dev/null
echo "eof st=$?"
set -- p q r s t u v w x y z aa bb cc dd
select z; do break; done </dev/null
COLUMNS=40
select z in "$@"; do echo $z; break; done <<< 5
select w in; do echo no; done; echo "empty st=$?"
f(){ select i in one two; do return 7; done; }; f <<< 1; echo "ret=$?"
select k in a b; do continue; done <<< $'1\n2'
echo done
