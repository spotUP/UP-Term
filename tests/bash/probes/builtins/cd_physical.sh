# cd -P and pwd -P give the physical path, links resolved; the last of -L -P wins
mkdir real; ln -s real link
t() { sed 's|.*/||'; }
cd link; pwd -P | t; pwd -L -P | t; cd ..
cd -P link; pwd | t; echo "$PWD" | t; pwd -P | t; cd ..
cd -P -L link; pwd -P | t; cd ..
cd -L -P link; echo "$PWD" | t; cd ..
cd -P link/. ; echo "$PWD" | t; echo "$OLDPWD" | t
cd -P . ; echo "$?"
