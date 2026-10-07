exec {fd}>f.txt; exec {fd2}>&$fd; echo "$fd $fd2"; exec {fd}>&-; echo z >&$fd2; exec {fd2}>&-; cat f.txt
