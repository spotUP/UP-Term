set -C; echo one > c.txt; echo two > c.txt 2>/dev/null; echo "rc=$?"; cat c.txt
echo three >| c.txt; cat c.txt; echo four >> c.txt; cat c.txt
echo five >| new_c.txt; cat new_c.txt; set +C; echo six > c.txt; cat c.txt
set -o noclobber; { echo x > c.txt; } 2>/dev/null; echo "rc=$?"; echo y >| c.txt; cat c.txt
