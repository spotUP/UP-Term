exec 2>&1
set -v
echo "$(echo sub)"
f() {
  echo in f
}
f
echo end \
continued
