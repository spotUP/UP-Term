set -- keep; echo "echo in:\$#:\$1" > sr.sh; . ./sr.sh a b; echo "out:$#:$1"; . ./sr.sh; echo "out2:$#:$1"
