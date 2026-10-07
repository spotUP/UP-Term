# V60: every P3 feature is reached through the shell's top-level entry; p3_reach.hits asserts the counters
echo {a,b}{1,2}
x=hello; echo ${x/l/L} ${x^^} ${x//l}
cat <<< "here-string"
cat <(echo procsub-in) | cat -n
echo out | tee >(cat -n) > /dev/null; sleep 0.3
cat <<EOF2
here-doc
EOF2
echo $(echo subst)
