cd dir; echo ${PWD##*/}; p=$(pwd -P); echo "${p##*/}"; p=$(pwd -L); echo "${p##*/}"
