exec 3>o.txt; echo hi >/dev/fd/3; echo there >>/dev/fd/3; exec 3>&-; cat o.txt
