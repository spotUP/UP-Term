# complete: specs stored, printed as bash prints them, removed (V93)
complete -W 'a b' -o nospace -F _f foo
complete -p foo
complete -d -f -c -a -v bar
complete -p bar
complete -C "echo hi" -W '$HOME x' baz
complete -p baz
complete -F f -C c -o default -o filenames -A function -b qux
complete -p qux
complete -p nosuch; echo "rc=$?"
complete -r foo; echo "rc=$?"
complete -p foo; echo "rc=$?"
complete -r bar nosuch; echo "rc=$?"
complete -o bogus x; echo "rc=$?"
complete -z x; echo "rc=$?"
compgen -A bogus; echo "rc=$?"
complete -W; echo "rc=$?"
complete -W x; echo "rc=$?"
complete -W one two
complete -W three two
complete -p two
complete -r
complete -p; echo "rc=$?"
