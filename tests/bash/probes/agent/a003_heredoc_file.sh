cat > out.txt <<'EOF'
line1 $HOME
line2
EOF
wc -l < out.txt | tr -d ' '; cat out.txt
