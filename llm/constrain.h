/* constrain -- strict JSON-schema tools, shared by the three request backends
 * (amiga-pi plan PX1), after pi-ai api/constrained-sampling.js.
 *
 * A tool whose declaration carries constrainedSampling {type:"json_schema",
 * strict:"prefer"|"require"} is sent in strict mode when the model supports
 * it (compat supportsStrictTools on anthropic-messages, supportsStrictMode on
 * openai-completions and openai-responses) and its schema fits the strict
 * subset; "prefer" falls back to a plain tool, "require" is an error.
 * The strict schema (makeStrictJsonSchema): every object gets
 * additionalProperties:false and all its properties in required, a property
 * that was optional becomes anyOf [property, {"type":"null"}].
 * Portable C89 over claude/json.h, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_CONSTRAIN_H
#define LLM_CONSTRAIN_H

#include "../claude/json.h"

/* the schema keywords Anthropic's strict mode refuses (isAnthropicStrictUnsupportedKeyword) */
enum { LCS_NONE, LCS_ANTHROPIC };

/* makeStrictJsonSchema: params (a tool's parameters) written strict to out; 0, -1 when the schema is outside the
 * strict subset (out is then unspecified), -2 out of memory */
int llm_strict_schema(jv params, jw *out, int kw);

/* resolveJsonSchemaStrictSampling for one tool declaration {name, description, parameters, constrainedSampling?}:
 * 1 the tool is sent strict (its strict parameters written to schema), 0 it is not, -1 "require" cannot be met
 * (err gets pi's text), -2 out of memory */
int llm_tool_strict(jv tool, int supports, int kw, jw *schema, char *err, long errcap);

#endif
