ls -1 src | while read f; do echo "${f%.*}"; done | sort -u
