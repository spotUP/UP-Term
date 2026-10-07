trap 'echo caught' INT; kill -INT $$; echo after
