[[ foo123bar =~ [0-9]+ ]] && echo "${BASH_REMATCH[0]}"; [[ foo123bar =~ ([a-z]+)([0-9]+) ]]; echo "${BASH_REMATCH[0]} ${BASH_REMATCH[1]} ${BASH_REMATCH[2]} ${#BASH_REMATCH[@]}"
