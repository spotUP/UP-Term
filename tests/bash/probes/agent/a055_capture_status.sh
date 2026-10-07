output=$(ls nosuchfile 2>&1) || echo "exit=$?"
