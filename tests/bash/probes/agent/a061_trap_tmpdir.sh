tmp=$(mktemp -d) && trap 'rm -rf "$tmp"' EXIT; echo made; [ -d "$tmp" ] && echo exists
