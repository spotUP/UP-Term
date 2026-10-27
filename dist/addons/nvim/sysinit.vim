" up-nvim: read by every nvim before the user's own init (as $VIM/sysinit.vim).
" The default 'shell' is sh, which AmigaOS does not have (:terminal and :! fail with
" "E475 ... 'sh' is not executable"). The add-on requires up-term.lha, which installs vsh
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

" Startup cost: on a 68060 these bundled plugins add about 1.35 s to every start
" (headless medians 6603 ms before, 5251 ms after; matchit 510 ms, netrw 620 ms,
" rplugin 100 ms, gzip 99 ms). Plugins load after the user's init, so a user who wants
" one back puts `unlet g:loaded_matchit` (or the matching name) in their own init.
let g:loaded_netrw = 1
let g:loaded_netrwPlugin = 1
let g:loaded_matchit = 1
let g:loaded_gzip = 1
let g:loaded_tarPlugin = 1
let g:loaded_zipPlugin = 1
let g:loaded_remote_plugins = 1
let g:loaded_tutor_mode_plugin = 1
