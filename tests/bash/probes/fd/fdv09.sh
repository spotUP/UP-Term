echo a >/dev/stdout 2>/dev/null | cat; echo b 2>&1 >/dev/stderr | cat
