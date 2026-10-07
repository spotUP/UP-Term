f=/path/to/file.tar.gz; echo "${f##*/} ${f%.gz} ${f%%.*} ${f#/}"
