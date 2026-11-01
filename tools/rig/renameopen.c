/*
 * renameopen: rename a file while a handle on it is still open, then
 * close the handle (s43 NVIM-HEAP-CORRUPT, FS-UAE filesys rename crash).
 *
 * FS-UAE's directory filesystem (filesys.cpp action_rename_object) moved
 * the inode to a new a_inode and freed the old one, but left every open
 * file handle (Key) still pointing at the freed one.  Only MODE_NEWFILE
 * handles were refused.  A handle opened with MODE_OLDFILE or
 * MODE_READWRITE that wrote data then crashed the emulator on Close
 * (notify_check on the freed a_inode).  Real FFS allows the rename and
 * the handle keeps working, so this test expects success throughout.
 *
 * Usage: renameopen [DIR]   (DIR on a host-directory volume, default VTC:)
 * Prints "renameopen PASS" when both modes survive and the data reads back.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

long __stack = 8192;
const char renameopen_stack_cookie[] = "$STACK: 8192";

static int one_case(const char *dir, LONG mode, const char *label)
{
	char a[256], b[256], buf[32];
	BPTR fh;
	LONG n;

	sprintf(a, "%s%srenameopen_a", dir, dir[strlen(dir) - 1] == ':' ? "" : "/");
	sprintf(b, "%s%srenameopen_b", dir, dir[strlen(dir) - 1] == ':' ? "" : "/");
	DeleteFile((STRPTR)a);
	DeleteFile((STRPTR)b);

	/* the file must exist for MODE_OLDFILE */
	fh = Open((STRPTR)a, MODE_NEWFILE);
	if (!fh) { printf("%s: create failed %ld\n", label, IoErr()); return 0; }
	Close(fh);

	fh = Open((STRPTR)a, mode);
	if (!fh) { printf("%s: open failed %ld\n", label, IoErr()); return 0; }
	if (Write(fh, (APTR)"hello", 5) != 5) { printf("%s: write failed\n", label); Close(fh); return 0; }
	printf("%s: open+write ok\n", label);
	if (!Rename((STRPTR)a, (STRPTR)b)) {
		printf("%s: rename refused %ld\n", label, IoErr());
		Close(fh);
		return 0;
	}
	printf("%s: rename ok\n", label);
	if (Write(fh, (APTR)"world", 5) != 5) printf("%s: write after rename failed\n", label);
	Close(fh); /* the unfixed emulator dies here */
	printf("%s: close ok\n", label);

	fh = Open((STRPTR)b, MODE_OLDFILE);
	if (!fh) { printf("%s: reopen failed %ld\n", label, IoErr()); return 0; }
	n = Read(fh, buf, sizeof(buf) - 1);
	Close(fh);
	buf[n < 0 ? 0 : n] = 0;
	DeleteFile((STRPTR)b);
	if (strcmp(buf, "helloworld") != 0) { printf("%s: content '%s'\n", label, buf); return 0; }
	printf("%s: content ok\n", label);
	return 1;
}

int main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : "VTC:";
	int ok = one_case(dir, MODE_OLDFILE, "oldfile");
	ok &= one_case(dir, MODE_READWRITE, "readwrite");
	printf("renameopen %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 10;
}
