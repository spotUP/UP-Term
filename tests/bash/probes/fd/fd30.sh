exec 3>f.txt; cat <<EOF >&3
heredoc
EOF
exec 3>&-; cat f.txt
