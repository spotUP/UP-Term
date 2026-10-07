exec 3>f.txt; exec 1>&3; echo redirected; exec 1>&2; exec 3>&-; cat f.txt
