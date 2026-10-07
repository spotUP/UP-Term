# ENV is read by an interactive posix shell only (expanded as a word); BASH_ENV by none of them in posix mode
printf 'echo env_ran\n' > e.sh
ENV=./e.sh $BASH --posix -i -c 'echo main' 2>/dev/null </dev/null
ENV=./e.sh $BASH --posix -c 'echo plain' 2>/dev/null </dev/null
ENV=./e.sh $BASH -i -c 'echo nonposix' 2>/dev/null </dev/null
ENV='${PWD}/e.sh' $BASH --posix -i -c 'echo expanded' 2>/dev/null </dev/null
BASH_ENV=./e.sh $BASH --posix -c 'echo benv' 2>/dev/null </dev/null
ENV=./nosuch.sh $BASH --posix -i -c 'echo missing' 2>/dev/null </dev/null
$BASH --login -c 'shopt login_shell; logout; echo not_reached' 2>&1 </dev/null | sed 's/^[^:]*: line [0-9]*: //;s/^vsh: //'
$BASH -c 'shopt login_shell'
