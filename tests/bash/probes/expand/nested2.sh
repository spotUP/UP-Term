a=; echo "${a:-"x y"}" "${a:-'q'}" "${a:-"it's"}" ${a:-"p  q"}
echo "${a:-"}"}" "${a:-")"}" "$(echo "a  b")"
b=set; echo "${b:+"yes  no"}" "${b:+'single'}"
echo "x${a:="d  e"}y" "$a"
echo "${a#"d "}" "${a%"e"}" "${a/"d "/X}"
