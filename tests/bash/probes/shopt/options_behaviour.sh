shopt -o -q noclobber; echo $?
set -o noclobber
shopt -o -q noclobber; echo $?
shopt -o noclobber errexit
shopt -po noclobber errexit
shopt -o nosuch 2>/dev/null; echo $?
shopt -s xpg_echo; echo 'a\tb'; echo -n 'c\n'; echo -e 'd\te'; shopt -u xpg_echo; echo 'a\tb'
echo 'echo sourced' > src1.sh
PATH=$PWD:$PATH
. src1.sh
shopt -u sourcepath
. src1.sh 2>/dev/null; echo $?
shopt -s sourcepath
set -e
x=$(false; echo after); echo "[$x]"
shopt -s inherit_errexit
x=$(false; echo after2); echo "[$x]"
set +e
shopt -q extglob; echo $?
shopt -s extglob; shopt -q extglob; echo $?
shopt -p extglob globstar expand_aliases inherit_errexit xpg_echo sourcepath globasciiranges checkwinsize
shopt -s histappend cdspell; shopt histappend cdspell
shopt -q nosuch; echo $?
shopt -s nosuch 2>/dev/null; echo $?
shopt | wc -l
shopt -s | head -20
