x="a b"; [[ $x == "a b" ]] && echo nosplit; [[ $x == a* && $x != b* ]] && echo andne; [[ ( 1 -eq 1 ) ]] && echo paren
