f() { echo out; echo err >&2; }
f &> b.txt; cat b.txt; f &>> b.txt; cat b.txt; : > b.txt; f &>> b.txt; cat b.txt
f &>/dev/null; echo rc=$?; f 2>&1 | wc -l; f >/dev/null 2>&1; echo rc=$?
f &>> /dev/null; { f; } &>> b2.txt; cat b2.txt
