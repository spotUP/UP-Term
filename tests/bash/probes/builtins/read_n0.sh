printf 'abc' | { read -n 0 x; echo "$?[$x]"; }
