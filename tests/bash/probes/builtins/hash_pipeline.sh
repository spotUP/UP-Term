# a pipeline's commands are hashed in their subshells: the shell's own table stays as it was
main() {
hash -r
ls tests >/dev/null 2>&1 | cat
hash
echo "st=$?"
ls . | wc -l >/dev/null
hash
echo "st=$?"
echo x | cat | cat >/dev/null
hash -l
echo "st=$?"
sort /dev/null
hash
}
main 2>/dev/null | sed "s|/usr/bin/|P|;s|usr:bin/|P|;s|/bin/|P|;s|bin:|P|"
