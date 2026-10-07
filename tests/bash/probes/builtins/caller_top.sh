caller; echo a=$?
caller 0; echo b=$?
caller 1; echo c=$?
f(){ source ./s.sh; }
printf "caller; caller 0\n" > s.sh
f
source ./s.sh; echo d
