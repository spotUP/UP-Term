# the error stream of a $( ) is the shell's, not the capture (Amiga: a background subshell and a
# pipeline's programs got their output as their error stream: messages landed in the captured text)
exec 2>/dev/null
x=$(echo out; echo err >&2); echo "x=[$x]"
x=$( { echo in-err >&2; } 2>&1 ); echo "x=[$x]"
y=$(cat nonexist_$$ | tr a b; echo end); echo "y=[$y]"
z=$(wc -c <nonexist_$$; echo end); echo "z=[$z]"
