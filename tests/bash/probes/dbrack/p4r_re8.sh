re="a.c"; [[ abc =~ "$re" ]] || echo quoted-var; [[ abc =~ $re ]] && echo var
