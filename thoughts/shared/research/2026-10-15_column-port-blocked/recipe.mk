# column (vtcon plan 4.9): OpenBSD's usr.bin/column (ISC/BSD), vendored unchanged in
# pkgs/column/src. Tried and left: util-linux's column (wants libsmartcols and
# libcommon; the first file fails on Linux-only _SC_HOST_NAME_MAX, useconds_t,
# reallocarray) and FreeBSD's (wide stdio: fgetws, wprintf, which ixemul lacks).
# pledge() is a no-op here (-D below). Source, OpenBSD master 2026-10-15:
#   https://raw.githubusercontent.com/openbsd/src/master/usr.bin/column/column.c
#   sha256 c0588ad23c359441646155ee3f29679081a96672bc18b18e81aaeb29dfed8d48
#   column.1 sha256 5a36da1bbbf55e6829799ca8b5d12a0e5d461c0ee36158bbec31f26c8d7b470c
column_OWN       := 1
column_VERSION   := openbsd-master
column_LICENSE   := ISC
column_DEPS      := ixcompat
column_BINS      := bin/column
column_STACK     := 65536
column_CHECK_DEPS := sed
column_CONFIGURE_CMD = mkdir -p $(column_OBJ)
column_BUILD_CMD = $(ROOT)/tools/m68k-cc $(CPU) -O2 -Wall -I$(SYSINC) -D"pledge(a,b)=0" -o $(column_OBJ)/column $(column_SRC)/column.c \
	-Wl,$(OBJ)/column.stack.o $(SYSLIB)/libixcompat.a
column_INSTALL_CMD = install -d $(SYSROOT)$(PREFIX)/bin $(SYSROOT)$(PREFIX)/share/man/man1 && \
	install -m 755 $(column_OBJ)/column $(SYSROOT)$(PREFIX)/bin/column && \
	install -m 644 $(column_SRC)/column.1 $(SYSROOT)$(PREFIX)/share/man/man1/column.1
column_HOST_CONFIGURE_CMD = mkdir -p $(column_HOBJ)
column_HOST_BUILD_CMD = cc -O2 -Wall -D"pledge(a,b)=0" -o $(column_HOBJ)/column $(column_SRC)/column.c && mkdir -p $(column_HOBJ)/inst/bin && \
	cp $(column_HOBJ)/column $(column_HOBJ)/inst/bin/column
