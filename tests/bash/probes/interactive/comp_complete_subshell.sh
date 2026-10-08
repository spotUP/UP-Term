# complete's specs reach a subshell and a pipeline stage, as bash's forked copies have them (V93)
complete -W 'a b' foo
complete -p | cat
(complete -p foo)
echo "$(compgen -W 'x y' x)"
