# A bare word that names a directory and no command: vsh runs cd on it, like the AmigaDOS Shell's
# implicit CD and zsh AUTO_CD (d7b76e11). bash without shopt -s autocd says "command not found" (127).
mkdir drawer_q
drawer_q 2>/dev/null
echo "drawer $?"
case $PWD in */drawer_q) echo "entered drawer_q" ;; *) echo "stayed" ;; esac
