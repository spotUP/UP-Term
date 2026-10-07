result=$(printf 'a\nb\nc\n' | grep -v b | tr '\n' ' '); echo "[$result]"
