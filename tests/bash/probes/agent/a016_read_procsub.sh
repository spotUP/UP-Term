while read -r k v; do echo "$k=$v"; done < <(printf 'a 1\nb 2\n')
