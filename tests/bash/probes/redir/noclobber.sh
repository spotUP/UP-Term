set -o noclobber; echo a > nc.txt; echo b > nc.txt 2>/dev/null; echo rc=$?; cat nc.txt
