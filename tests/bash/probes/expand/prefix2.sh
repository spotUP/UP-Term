pa=1; pb=2; pc=3; qq=4
echo ${!p*} "${!p*}" "${!p@}"
for v in "${!p@}"; do echo "<$v>"; done
IFS=:; echo "${!p*}" "${!p@}"; unset IFS
echo "[${!zz*}]" "[${!zz@}]"
arr=(1 2); echo ${!ar*}
set --; echo ${#@} ${#*} $#
set -- a bb ccc; echo ${#@} ${#*} "${#@}"
ref=pa; echo ${!ref} "${!ref}"
