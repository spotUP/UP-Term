/* schema -- a tool input checked against the JSON schema the tool declares
 * (ledger A4 WP2): the declared tools JSON is the one source of truth, no
 * second table of properties. The subset the tools use: type (object,
 * string, number, integer, boolean, array), properties, required,
 * additionalProperties false, enum, items. Messages name the parameter as
 * Claude Code's do ("The required parameter `file_path` is missing").
 * Portable C89, host-tested (tests/test_claude_tools.c). */
#ifndef CL_SCHEMA_H
#define CL_SCHEMA_H

#include "json.h"

/* 0, or -1 with the reason in err */
int schema_check(jv schema, jv value, char *err, long cap);

#endif
