re="^[a-z]+$"; [[ abc =~ $re ]] && echo m1; [[ ABC =~ $re ]] || echo m2; re="a.c"; [[ abc =~ $re ]] && echo m3
