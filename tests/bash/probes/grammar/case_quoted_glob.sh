# quoted or escaped glob characters in a case pattern and in ${x#pat} match themselves
t() { case abc in $1) echo "$1 yes";; *) echo "$1 no";; esac; }
case abc in "?"*) echo q1 yes;; *) echo q1 no;; esac
case abc in \?*) echo q2 yes;; *) echo q2 no;; esac
case abc in '?'*) echo q3 yes;; *) echo q3 no;; esac
case abc in "*") echo q4 yes;; *) echo q4 no;; esac
case "[" in "[") echo q5 yes;; *) echo q5 no;; esac
case "?x" in "?"*) echo q6 yes;; *) echo q6 no;; esac
p='?'
case abc in "$p"*) echo q7 yes;; *) echo q7 no;; esac
case abc in $p*) echo q8 yes;; *) echo q8 no;; esac
case abc in ?*) echo w1 yes;; *) echo w1 no;; esac
x=aab
echo "${x#"?"}" "${x#\?}" "${x#?}" "${x/"?"/Z}" "${x#"$p"}" "${x#$p}" ${x#?}
