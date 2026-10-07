shopt -s extglob
mkdir -p r && cd r && touch a.c b.h
echo @(*.c|*.h)
