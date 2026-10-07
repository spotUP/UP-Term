f1() { echo a b; ls -l | wc -l; }
f2() { if true; then echo y; elif false; then echo z; else echo n; fi; }
f3() { for i in 1 2 3; do echo $i; done; for j; do :; done; }
f4() { while read x; do echo "$x"; done < f; until false; do break; done; }
f5() { case $1 in a|b) echo ab;; c) echo c;& *) echo other;; esac; }
f6() { ( cd /; pwd ); { echo g; echo h; } > o 2>&1; }
f7() { a=1 b=2 cmd x; x=$(echo hi); echo "${a:-d}" 'sq' $'dq'; }
f8() { [[ -f x && ! -d y || $a == b* ]]; (( i++ )); for (( i=0; i<3; i++ )); do :; done; }
f9() { cat <<EOF
here $a
EOF
echo after; }
f10() { cat <<-'END'
	tab
	END
}
f11() { echo a & echo b; ! true; true && false || true; }
f12() { local x=1; function inner { :; }; inner; }
function f13 { echo 13; }
f15() { echo a; } 
f16() ( echo sub )
f17() { cat < f > o >> p 2>&1 <&3 4>&- &>x; }
f18() { cmd <<< word; exec 3<&0; }
f19() { if a; then b; fi; if c
then d
fi; }
f20() { while a; do b; done; }
f21() { :; }
f22() { echo "a
b"; }
f23() { echo a | { cat; } | ( cat ); }
f24() { if true; then if false; then :; fi; fi; }
f25() { { :; }; ( : ); }
for f in f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f15 f16 f17 f18 f19 f20 f21 f22 f23 f24 f25; do declare -f $f; done
type f2 f9
declare -F f1
