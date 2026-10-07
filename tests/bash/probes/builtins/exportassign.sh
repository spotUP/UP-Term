export A=1 B=2; env | grep -E '^(A|B)=' | sort
