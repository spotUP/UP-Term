# an interactive shell on a terminal sets COLUMNS and LINES from its size, and again after a command (checkwinsize; V42)
echo "[$COLUMNS $LINES]"
/bin/echo external
echo "[$COLUMNS $LINES]"
shopt checkwinsize
