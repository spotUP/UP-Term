[ "$PPID" -gt 0 ] && echo ppid
[ "$UID" = "$(id -u)" ] && echo uid
[ "$EUID" = "$(id -u)" ] && echo euid
(UID=5) 2>/dev/null; echo $?
[ "$BASHPID" -gt 0 ] && echo pid
[ "$BASHPID" = "$$" ] && echo same
