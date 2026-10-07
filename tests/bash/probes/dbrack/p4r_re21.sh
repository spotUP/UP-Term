[[ abc =~ ^(a|b)+ ]]; echo "${BASH_REMATCH[0]}"; [[ "" =~ ^$ ]] && echo empty
