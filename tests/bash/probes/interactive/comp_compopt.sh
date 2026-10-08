# compopt: a spec's -o options changed and listed; outside a completion function it fails (V93)
complete -o default -W 'a' x
compopt -o nospace x
complete -p x
compopt +o default x
complete -p x
compopt x
compopt nosuch; echo "rc=$?"
compopt -o nospace; echo "rc=$?"
compopt -z; echo "rc=$?"
