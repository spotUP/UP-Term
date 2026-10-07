shopt -s extglob
mkdir -p ey && cd ey
touch .hid .x a
echo @(.hid) @(a|.x) !(.hid) @(b|.h*) *(.hid) ?(.x) +(.x)
echo @(a)
