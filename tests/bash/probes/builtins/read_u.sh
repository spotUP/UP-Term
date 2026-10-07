echo hi | { read -u 0 x; echo $x; }; echo yo | { read -ru0 x; echo $x; }
