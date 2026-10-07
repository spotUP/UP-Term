exec 2>/dev/null
mkdir -p a/b c d
R=$(pwd -P)
N(){ sed "s|$R|R|g"; }
{
pushd a >/dev/null; echo "rc=$?"
pushd b; echo "rc=$?"
dirs; dirs -p; dirs -v; dirs -l
pushd; echo "rc=$?"
pwd | sed "s|$R|R|"
pushd +1 | sed "s|$R|R|g"
popd | sed "s|$R|R|g"; echo "rc=$?"
popd +0 | sed "s|$R|R|g"
pushd $R/c >/dev/null; pushd $R/d >/dev/null
dirs -p | sed "s|$R|R|g"
popd +1 >/dev/null; dirs -p | sed "s|$R|R|g"
pushd -0 >/dev/null; dirs -p | sed "s|$R|R|g"
pushd -n $R/a >/dev/null; dirs -p | sed "s|$R|R|g"
pushd nosuch; echo "rc=$?"
popd >/dev/null; popd >/dev/null; popd; echo "rc=$?"
pushd; echo "rc=$?"
dirs -c; dirs
} 2>/dev/null | N
