bind -v | grep editing-mode
set -o vi
bind -v | grep editing-mode
bind 'set editing-mode emacs'
set -o | grep -E '^(vi|emacs) '
bind '"\C-a": end-of-line'; echo "rc=$?"
bind '"\C-a": no-such-function' 2>/dev/null; echo "rc=$?"
