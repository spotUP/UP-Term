echo x >/dev/fd/9 2>/dev/null; echo st=$?; read y </dev/fd/9 2>/dev/null; echo st=$?
