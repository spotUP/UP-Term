[[ "a b" =~ a\ b ]] && echo sp1; [[ "a b" =~ "a b" ]] && echo sp2; [[ "a b" =~ (a b) ]] && echo sp3
