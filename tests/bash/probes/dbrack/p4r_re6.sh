[[ "a.c" =~ a\.c ]] && echo dot1; [[ abc =~ a\.c ]] || echo dot2; [[ abc =~ "a.c" ]] || echo quoted-literal; [[ "a.c" =~ "a.c" ]] && echo quoted-same
