printf 'echo env_ran $-\nBEV=set\n' > e.sh
BASH_ENV=./e.sh $BASH -c 'echo main $BEV'
BASH_ENV=./e.sh $BASH e2.sh 2>/dev/null; echo "st=$?"
printf 'echo script\n' > e2.sh
BASH_ENV=./e.sh $BASH e2.sh
BASH_ENV=./missing.sh $BASH -c 'echo nomissing'
BASH_ENV= $BASH -c 'echo empty'
BASH_ENV=./e.sh $BASH -i -c 'echo interactive' 2>/dev/null </dev/null
logout 2>&1 | sed 's/^[^:]*: line [0-9]*: //;s/^vsh: //'; echo "st=$?"
