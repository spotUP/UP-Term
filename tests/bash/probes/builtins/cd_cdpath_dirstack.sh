mkdir -p a/b c d
base=$PWD
CDPATH=c:a; cd b; echo "st=$? ${PWD#$base}"; cd "$base"
CDPATH=/nonexist:./a cd b >/dev/null; echo "st=$?"
cd "$base"; CDPATH=a; cd b | cat; echo "${PWD#$base}"
cd ./b 2>/dev/null; echo "st=$? ${PWD#$base}"; cd "$base"
CDPATH=:a cd d; echo "st=$? ${PWD#$base}"; cd "$base"
CDPATH=a:; cd b; echo "${PWD#$base}"; cd "$base"; unset CDPATH
cd -P a; echo "${PWD#$base}"; cd -L ..; echo "${PWD#$base}"; cd -P -- a/b; echo "${PWD#$base}"
cd -; echo "st=$?"
cd "$base"; cd -x 2>/dev/null; echo "st=$?"
cd a b 2>/dev/null; echo "st=$?"
pwd -P | sed "s|$base|B|"; pwd -L | sed "s|$base|B|"; pwd -x 2>/dev/null; echo "st=$?"
pushd a >/dev/null; pushd b >/dev/null
echo "${#DIRSTACK[@]}"; for d in "${DIRSTACK[@]}"; do echo "${d#$base}"; done
echo "${DIRSTACK[1]#$base}"
popd >/dev/null; echo "${#DIRSTACK[@]} ${DIRSTACK[0]#$base}"
