mkdir -p lib; echo "echo sourced \$1" > lib/s.sh; PATH=$PWD/lib:$PATH . s.sh arg1; echo rc=$?
