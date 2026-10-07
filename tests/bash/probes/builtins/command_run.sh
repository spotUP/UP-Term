echo() { printf "fn\n"; }; command echo hi; builtin echo hi2; echo hi3; builtin nonesuch 2>/dev/null; echo rc=$?
