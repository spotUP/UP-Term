exec 5>>ap.txt; echo 1 >&5; echo 2 >&5; exec 5>&-; cat ap.txt
