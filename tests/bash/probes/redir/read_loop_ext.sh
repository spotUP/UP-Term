printf "%s\n" one two three > data
while read l; do
    echo "got:$l"
    true an argument line
    ls -d . > /dev/null
done < data
echo "status $?"
