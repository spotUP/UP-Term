exec 4>/dev/fd/1; echo viafour >&4; exec 4>&-; echo done
