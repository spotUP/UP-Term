#ifndef UPCONF_LEX_H
#define UPCONF_LEX_H
/* The line helpers upconf.c and upconf_rows.c share (upconf_rows.c defines them). */
int   uc_ieq(const char *a, const char *b);
int   uc_copy(char *dst, const char *src, int n);
char *uc_section_name(char *s);
char *uc_key_value(char *s, char **value);
#endif
