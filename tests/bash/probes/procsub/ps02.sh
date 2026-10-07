printf "a\nb\n" > f1; printf "a\nc\n" > f2; diff <(cat f1) <(cat f2); echo status $?
