set -e; [ -o errexit ]; echo $?; set +e; [ -o errexit ]; echo $?; [ -o nosuchopt ]; echo $?; set -o noglob; [ -o noglob ]; echo $?
