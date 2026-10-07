s=$'a\nb'; [[ $s =~ a.b ]] && echo dotnl; [[ $s =~ ^b ]] || echo nobol; [[ $s =~ a$ ]] || echo noeol
