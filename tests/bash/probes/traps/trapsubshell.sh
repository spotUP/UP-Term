trap 'echo outer-exit' EXIT; (trap 'echo sub-exit' EXIT; echo in-sub); echo main
