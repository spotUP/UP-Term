set -e; false || true; echo survived; (false; echo not) || echo caught
