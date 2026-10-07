a=(x y z); echo ${a[1]} ${#a[@]} "${a[@]}"; a+=(w); echo ${#a[@]}; unset a[0]; echo ${a[@]}
