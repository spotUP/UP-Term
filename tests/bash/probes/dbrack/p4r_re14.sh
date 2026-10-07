[[ abc =~ (b)(x)? ]]; echo "${#BASH_REMATCH[@]} [${BASH_REMATCH[1]}] [${BASH_REMATCH[2]}]"
