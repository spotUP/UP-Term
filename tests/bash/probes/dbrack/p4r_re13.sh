[[ abc123 =~ [[:alpha:]]+[[:digit:]]+ ]] && echo "${BASH_REMATCH[0]}"; [[ a1 =~ ^[[:alnum:]]+$ ]] && echo alnum; [[ "a b" =~ [[:space:]] ]] && echo space
