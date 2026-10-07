for (( i=0; i<5; i++ )); do (( i == 2 )) && continue; echo $i; done
