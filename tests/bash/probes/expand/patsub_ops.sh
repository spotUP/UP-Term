x=hello; echo ${x/l/L} ${x//l/L} ${x/xyz/Q} ${x/#he/HE} ${x/%lo/LO} ${x/#z/Z} ${x//l} ${x/l*/[&]} ${x//l/\&} ${x/hello/}
y=Hello; echo ${y^^} ${y,,} ${y^} ${y,} ${y~} ${y~~} ${y^^[el]} ${y,,[H]}
a=(foo bar baz); echo ${a[@]/a/A} "${a[@]//a/_}" ${a[@]#?} ${a[@]%%a*} ${a[@]^}
set -- ab cb; echo ${@/b/X} ${*^^} "${@#a}" ${x//[el]/-}
