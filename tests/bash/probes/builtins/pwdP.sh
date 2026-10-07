cd dir; echo ${PWD##*/}; pwd -P | sed 's|.*/||'; pwd -L | sed 's|.*/||'
