shopt -s nocasematch
case ABC in abc) echo case-ci;; esac
[[ ABC == a* ]] && echo db-ci
shopt -u nocasematch
case ABC in abc) echo no;; *) echo case-cs;; esac
