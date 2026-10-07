set -o pipefail; false | true; echo $?; true | false | true; echo $?; (exit 2) | (exit 3) | true; echo $?
