f() { echo out; echo err >&2; }
f |& sort; f |& cat | wc -l; { f; } |& tr a-z A-Z; f 2>/dev/null |& wc -l
echo a | cat |& cat; ( f ) |& sort -r
if f |& grep -q err; then echo found; fi
