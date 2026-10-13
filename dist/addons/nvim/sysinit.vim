" UP-Term-nvim: read by every nvim before the user's own init (as $VIM/sysinit.vim).
" The default 'shell' is sh, which AmigaOS does not have (:terminal and :! fail with
" "E475 ... 'sh' is not executable"). The add-on requires UP-Term.lha, which installs vsh
" in UP-Term:bin and C:. A shell the user set in their own init (or $SHELL) still wins:
" this file runs first, and the default is only replaced while nothing else named one.
if empty($SHELL)
  if executable('UP-Term:bin/vsh')
    set shell=UP-Term:bin/vsh
  elseif executable('C:vsh')
    set shell=C:vsh
  endif
endif
