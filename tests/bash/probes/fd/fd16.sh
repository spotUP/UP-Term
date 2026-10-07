exec {a}>fa.txt {b}>fb.txt; echo "$a $b"; echo A >&$a; echo B >&$b; exec {a}>&- {b}>&-; cat fa.txt fb.txt
