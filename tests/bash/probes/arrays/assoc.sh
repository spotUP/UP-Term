declare -A m; m[k]=v; m[j]=w; echo ${m[k]} ${#m[@]}; for k in "${!m[@]}"; do echo $k; done | sort
