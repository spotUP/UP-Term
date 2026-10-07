exec 3>&1 4>&2; echo a >&3; echo b >&4; exec 3>&- 4>&-
