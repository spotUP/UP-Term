set -o pipefail
echo start
[ nosuch1 -nt nosuch2 ] || echo stale
false | true
echo unreachable
