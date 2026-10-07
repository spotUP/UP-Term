x=hello; [[ $x =~ ^h(.*)o$ ]] && echo "${BASH_REMATCH[1]}"; [[ $x =~ ^(h)(e)(l)(l)(o)$ ]] && echo "${BASH_REMATCH[5]}"
