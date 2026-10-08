# compgen -W: the list split outside quotes, expanded without globbing, filtered by the word (V93)
compgen -W "b a b" ""; echo "rc=$?"
compgen -W "alpha beta bravo" b; echo "rc=$?"
compgen -W "x y" z; echo "rc=$?"
x=exp
compgen -W '$x "two words" $UNDEF d\ e' ""; echo "rc=$?"
compgen -W '*' ""; echo "rc=$?"
compgen -W "a b" -- a; echo "rc=$?"
compgen; echo "rc=$?"
