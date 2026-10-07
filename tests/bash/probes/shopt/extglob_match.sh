shopt -s extglob
mkdir -p ex && cd ex
touch a.txt b.txt ab.txt abab.txt c.c .hid aa.txt
echo @(a|b).txt
echo ?(a|b).txt
echo *(a|b).txt
echo +(a|b).txt
echo !(a|b).txt
echo !(*.txt)
echo !(a).txt
echo *(ab)
echo +(ab|a).txt
echo a@(b|)*.txt
echo @(a|b)@(a|b).txt
echo @(*.c|*.txt)
echo !(*.c|*.txt)
echo "@(a|b).txt"
echo @("a"|b).txt
echo @(a\|b).txt
echo @(a[b-c]|b).txt
x=ab; case $x in @(a|b)b) echo m1;; esac
case $x in !(a)) echo m2;; *) echo nm2;; esac
case abab in +(ab)) echo m3;; esac
case "" in *(a)) echo m4;; esac
case a in ?(a)) echo m5;; esac
[[ foo.c == @(*.c|*.h) ]] && echo m6
[[ foo.o == !(*.c|*.h) ]] && echo m7
[[ foo.c == !(*.c|*.h) ]] || echo m8
[[ aab == +(a)b ]] && echo m9
[[ ab == !(a)b ]] && echo m10
[[ b == !(a)b ]] && echo m11
v=abcabc; echo ${v#@(abc)} ${v##*(abc)} ${v%@(bc|c)} ${v/@(b|c)/_} ${v//+(b|c)/_}
echo @(nomatch|x)
echo @(.hid) !(a*).hid
shopt -s dotglob; echo @(.h*) !(a*|b*|c*)
shopt -u dotglob
shopt -s nullglob; echo n@(zz)n; shopt -u nullglob
shopt -s nocaseglob; echo @(A|B).txt; shopt -u nocaseglob
shopt -u extglob
eval 'echo @(a|b).txt' 2>/dev/null; echo $?
