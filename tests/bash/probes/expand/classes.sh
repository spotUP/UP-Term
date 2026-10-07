echo [[:lower:]].txt [[:alpha:]]*.txt [[:digit:]]*.txt [![:digit:]].txt [[:upper:]]*.txt s[[:digit:]].sh *.[[:alpha:]][[:alpha:]]
echo [[:alnum:]_].txt [^[:space:]]?.txt [[:punct:]]* [a[:digit:]]*.txt
for w in a Z 5 _ " " "	" "!" é; do case "$w" in [[:alpha:]]) t=alpha;; [[:digit:]]) t=digit;; [[:space:]]) t=space;; [[:punct:]]) t=punct;; *) t=other;; esac; printf '%s ' "$t"; done; echo
x=Hello1; echo "${x//[[:upper:]]/_}" "${x//[[:alpha:]]/}" "${x#[[:upper:]]}" "${x%%[[:digit:]]}"
for c in a A 1 ' ' x '#'; do for cl in alpha digit alnum upper lower space blank punct print graph cntrl xdigit; do eval "case \"\$c\" in [[:$cl:]]) printf '%s' 1;; *) printf '%s' 0;; esac"; done; echo; done
echo [[:bogus:]]x [[:alpha:] [[:alpha:]
shopt -s nocaseglob 2>/dev/null; echo [[:upper:]].txt
