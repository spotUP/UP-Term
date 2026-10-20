" up-nvim: read by every nvim before the user's own init (as $VIM/sysinit.vim).
" The default 'shell' is sh, which AmigaOS does not have (:terminal and :! fail with
" "E475 ... 'sh' is not executable"). The add-on requires UP-Term.lha, which installs vsh
" in UP-Term:bin and C:. nvim (ixemul) tests the Unix form of those paths:
" executable('C:vsh') is 0 on the rig, executable('/c/vsh') is 1.
" A shell the user names in their own init still wins (this file runs first), and so does
" $SHELL: the default is replaced only while $SHELL is unset. getenv(), not $SHELL: an
" unset $SHELL in an expression fails to parse here.
if empty(getenv('SHELL'))
  if executable('/UP-Term/bin/vsh')
    set shell=/UP-Term/bin/vsh
  elseif executable('/c/vsh')
    set shell=/c/vsh
  endif
endif
