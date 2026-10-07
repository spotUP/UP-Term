type echo; type if; type -p ls | sed "s|.*[/:]||"; type nonexist 2>/dev/null; echo rc=$?
