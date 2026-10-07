s=$(printf '%(%Y)T' 0); echo $s; printf -v v '%(%m/%d)T' 1700000000; echo $v
