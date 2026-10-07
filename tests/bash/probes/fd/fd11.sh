{ echo scoped >&3; } 3>s3.txt; cat s3.txt; echo again >&3; echo "st=$?"
