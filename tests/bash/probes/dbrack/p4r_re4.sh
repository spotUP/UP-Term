[[ abc =~ x ]]; echo "rc=$? n=${#BASH_REMATCH[@]}"; [[ abc =~ b ]]; echo "rc=$? n=${#BASH_REMATCH[@]}"; [[ abc =~ x ]]; echo "n=${#BASH_REMATCH[@]}"
