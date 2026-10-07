[[ aXb =~ a(.)b ]]; echo "${BASH_REMATCH[1]}"; [[ ab =~ a(.)b ]] || echo none
