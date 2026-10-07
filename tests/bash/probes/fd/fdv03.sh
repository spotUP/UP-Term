echo hello | { read x </dev/stdin; echo "got $x"; }
