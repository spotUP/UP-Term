for p in "1 -eq 1" "1 -ne 2" "1 -lt 2" "2 -le 2" "3 -gt 2" "2 -ge 3" "-1 -lt 0" "10 -gt 9"; do set -- $p; [ $1 $2 $3 ]; echo "$p $?"; done
