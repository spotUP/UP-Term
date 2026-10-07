[[ foo.txt =~ \.txt$ ]] && echo ext; [[ foo.txt =~ ^(.*)\.txt$ ]] && echo "${BASH_REMATCH[1]}"
