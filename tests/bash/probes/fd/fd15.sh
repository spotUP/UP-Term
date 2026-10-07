exec {fd}>fv.txt; echo "fd=$fd"; echo x >&$fd; exec {fd}>&-; cat fv.txt
