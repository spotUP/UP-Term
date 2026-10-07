trap 'echo e' EXIT; trap 'echo x' USR1 INT; trap -p USR1; trap -p; trap -p TERM; echo rc=$?
trap '' HUP; trap -- 'a b' SIGUSR2 3; trap
trap - USR1 INT; trap USR2; trap -p 2>/dev/null; trap xyz FOO 2>/dev/null; echo rc=$?
trap 'echo q' 0; trap -p EXIT
trap 'echo x' KILL STOP; echo rc=$?
trap -l | head -3
trap 'echo got-usr2' SIGUSR2; kill -USR2 $$; kill -s USR2 $$; kill -31 $$; trap '' USR2; kill -USR2 $$; echo ignored
