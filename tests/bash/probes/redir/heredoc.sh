x=world
cat <<EOF
hello $x
$(echo sub) $((1+2))
EOF
