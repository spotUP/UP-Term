---
date: 2026-10-13
topic: Draft email to the ixemul 80.x maintainer (owner sends it)
tags: [ixemul, upstream, draft]
status: draft
---

To: Jörgen Johansson (Aminet uploader of dev/lib/ixemul-80.1-m68k.lha; address in the readme reads "jj201 telia com")
Subject: ixemul 80.1: patches you may want, and a version label question

Hello Jörgen,

Thank you for ixemul 80.0 and 80.1. I build UP-Term, a terminal and Unix userland for AmigaOS 3.x, and I have been
running a patched 80.1 for a few weeks. The source is public: https://github.com/spotUP/ixemul-vtcon

About 45 of the changes are general fixes that may be useful to every ixemul user, for example:
- AF_UNIX socketpair() and SCM_RIGHTS (Neovim's in-process UI needs socketpair; stock returns EPFNOSUPPORT)
- non-seekable streams treated as FIFOs, command-line parsing and execve/system fixes, small POSIX fixes
- header fixes (socklen_t guards and others)

1. Would you accept such patches, and in what form (diffs by mail, a git repository, one topic at a time)?
2. Until then I would like to publish my build on Aminet so Neovim and other ports run without UP-Term. I would keep
   your version number and mark it as "80.1 [vtcon-1]", with full source and change notices, and say clearly that it
   does not replace your release. Is that acceptable to you, or is there a version number you would rather I use?

Best regards,
spotUP
