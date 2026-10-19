/* main returns -1, as opkg/upkg do on error: vsh must show status 255, not "not found" (vshexec_rig.py case 5). */
int main(void) { return -1; }
