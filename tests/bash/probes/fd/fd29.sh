exec 3<a.txt; mapfile -u 3 arr; echo "${#arr[@]} ${arr[0]}"; exec 3<&-
