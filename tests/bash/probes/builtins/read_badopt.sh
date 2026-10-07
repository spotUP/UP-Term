{ read -Z x; echo $?; } 2>/dev/null; { read -u 9 x; echo $?; } 2>/dev/null
