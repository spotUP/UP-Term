exec {fd}>fv.txt; echo hi >&$fd; exec {fd}>&-; cat fv.txt
