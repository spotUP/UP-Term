exec 2>/dev/null
mkdir -p a/b c d
R=$(pwd -P)
N(){ while IFS= read -r l; do echo "${l//$R/R}"; done; }
{
pushd a >/dev/null; echo "rc=$?"
pushd b; echo "rc=$?"
dirs; dirs -p; dirs -v; dirs -l
pushd; echo "rc=$?"
pwd | N
pushd +1 | N
popd | N; echo "rc=$?"
popd +0 | N
pushd $R/c >/dev/null; pushd $R/d >/dev/null
dirs -p | N
popd +1 >/dev/null; dirs -p | N
pushd -0 >/dev/null; dirs -p | N
pushd -n $R/a >/dev/null; dirs -p | N
pushd nosuch; echo "rc=$?"
popd >/dev/null; popd >/dev/null; popd; echo "rc=$?"
pushd; echo "rc=$?"
dirs -c; dirs
} 2>/dev/null | N
