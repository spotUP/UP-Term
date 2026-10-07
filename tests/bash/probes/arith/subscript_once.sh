# an array element's subscript is evaluated once, whatever the operator
a=(0 0 0 0); i=0
((a[i++] += 5)); echo "${a[*]} i=$i"
i=0; ((a[i++]++)); echo "${a[*]} i=$i"
i=0; ((a[i++] = 7)); echo "${a[*]} i=$i"
i=0; x=$((a[i++] *= 3)); echo "${a[*]} i=$i x=$x"
j=0; ((a[j=2] -= 1)); echo "${a[*]} j=$j"
i=3; ((a[i--]--)); echo "${a[*]} i=$i"
i=0; y=$((a[i++] + a[i++])); echo "y=$y i=$i"
i=0; ((0 && a[i++] > 0)); echo "i=$i"
let 'a[i++] += 1'; echo "${a[*]} i=$i"
i=1; ((a[i++ - 1] += 10)); echo "${a[*]} i=$i"
((a[-1] += 1)); echo "${a[*]}"
