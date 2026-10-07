echo first > f.txt; { echo o; echo e >&2; } &>> f.txt; sort f.txt
