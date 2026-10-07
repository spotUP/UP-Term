f() { for (( k=0; k<3; k++ )); do (( k == 1 )) && return 4; done; }; f; echo $?
