x="a*"; [[ abc == $x ]] && echo glob; [[ abc == "$x" ]] || echo literal
