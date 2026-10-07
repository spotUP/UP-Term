{ echo out >/dev/stdout; echo err >/dev/stderr; echo more >/dev/fd/1; echo e2 >/dev/fd/2; } 2>&1 | cat
