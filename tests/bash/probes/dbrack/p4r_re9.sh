[[ ab =~ (a|ab) ]]; echo "${BASH_REMATCH[0]}"; [[ abcd =~ a|abcd ]]; echo "${BASH_REMATCH[0]}"
