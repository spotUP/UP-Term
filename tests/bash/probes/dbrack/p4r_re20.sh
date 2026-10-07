if [[ "v1.2.3" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then echo "${BASH_REMATCH[1]} ${BASH_REMATCH[2]} ${BASH_REMATCH[3]}"; fi
