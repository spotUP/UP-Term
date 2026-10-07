# a writer that ends without writing keeps its own status when the reader is gone first
set -o pipefail
(exit 2) | (exit 3) | true
echo "a=$? ${PIPESTATUS[*]}"
(exit 4) | true
echo "b=$? ${PIPESTATUS[*]}"
