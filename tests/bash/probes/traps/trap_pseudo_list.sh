trap 'echo e' ERR
trap 'echo d' DEBUG
trap 'echo r' RETURN
trap 'echo x' EXIT
trap 'echo u' USR1
trap -p ERR
trap -p DEBUG RETURN
trap - DEBUG
trap -p
trap '' ERR
trap -p ERR
trap - ERR RETURN
trap -p
trap 'echo n' SIGUSR1 usr1 2>&1
trap -p USR1
trap 'echo h' 1 SIGINT
trap -p HUP INT
