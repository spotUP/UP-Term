echo ${BASH_VERSINFO[0]} ${#BASH_VERSINFO[@]}
case $BASH_VERSION in 5.*) echo five;; esac
echo "${BASH_VERSINFO[0]}.${BASH_VERSINFO[1]}" | grep -q '^5\.' && echo ok
(BASH_VERSINFO=1) 2>/dev/null; echo ${BASH_VERSINFO[0]}
