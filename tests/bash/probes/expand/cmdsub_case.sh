x=$(case a in a) echo yes;; b) echo no;; esac); echo $x
y=$(case $x in (yes) echo paren;; esac); echo $y
echo $(echo ")") "$(echo '(')" $(echo a; case b in a) echo 1;; *) echo 2;; esac)
echo $(for i in 1 2; do case $i in 1) echo one;; *) echo other;; esac; done)
echo "$(echo "$(echo "deep  er")")"
