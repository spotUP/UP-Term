/* constrain -- see constrain.h. A port of pi-ai api/constrained-sampling.js makeJsonSchemaNodeStrict, writing
 * the strict schema as it goes: members keep their order, required / additionalProperties are replaced where
 * pi's code sets them (an absent one is appended), an optional property is wrapped in place. */
#include <stdlib.h>
#include <string.h>
#include "../claude/util.h"
#include "constrain.h"

#define CS_DEEP 40

static const char *const unsupported[] = {"$ref", "$defs", "definitions", "allOf", "oneOf", "patternProperties",
                                          "dependentSchemas", "dependencies", "unevaluatedProperties",
                                          "propertyNames", "contains", "prefixItems", "not", "if", "then", "else", 0};
static const char *const ant_keys[] = {"minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum", "multipleOf",
                                       "maxItems", "uniqueItems", "minContains", "maxContains", "minProperties",
                                       "maxProperties", 0};
static const char *const ant_formats[] = {"date-time", "time", "date", "duration", "email", "hostname", "uri",
                                          "ipv4", "ipv6", "uuid", 0};

static int in_list(const char *const *l, jv k)
{
    int i;
    for (i = 0; l[i]; i++)
        if (json_streq(k, l[i]))
            return 1;
    return 0;
}

/* isAnthropicStrictUnsupportedKeyword */
static int ant_unsupported(jv key, jv val)
{
    if (in_list(ant_keys, key))
        return 1;
    if (json_streq(key, "minItems")) {
        /* value !== 0 && value !== 1: written 0, -0, 1, with a zero fraction (no floating point: the Amiga build has
         * none linked in); an exponent form (1e0) counts as another number, which JavaScript would read as 1 */
        long i = 0;
        if (json_type(val) != J_NUM)
            return 1;
        if (val.p[0] == '-')
            i = 1;
        if (i < val.n && (val.p[i] == '0' || (val.p[i] == '1' && i == 0)))
            i++;
        else
            return 1;
        if (i < val.n && val.p[i] == '.') {
            for (i++; i < val.n && val.p[i] == '0'; i++)
                ;
        }
        return i != val.n;
    }
    if (json_streq(key, "format"))
        return json_type(val) != J_STR || !in_list(ant_formats, val);
    return 0;
}

static int is_type(jv s, const char *t)
{
    jv v;
    return json_type(s) == J_OBJ && json_get(s, "type", &v) && json_streq(v, t);
}

/* types.includes(t): type is the string, or an array holding it */
static int has_type(jv s, const char *t)
{
    jv v, e;
    jit it;
    if (json_type(s) != J_OBJ || !json_get(s, "type", &v))
        return 0;
    if (json_type(v) == J_STR)
        return json_streq(v, t);
    if (json_type(v) == J_ARR) {
        json_iter(v, &it);
        while (json_next(&it, 0, &e))
            if (json_type(e) == J_STR && json_streq(e, t))
                return 1;
    }
    return 0;
}

/* isStructuredSchema */
static int structured(jv s)
{
    jv v;
    if (json_type(s) != J_OBJ)
        return 0;
    return has_type(s, "object") || has_type(s, "array") || json_get(s, "properties", &v) || json_get(s, "items", &v);
}

/* schemaAllowsNull */
static int allows_null(jv s, int depth)
{
    jv v, e;
    jit it;
    if (json_type(s) != J_OBJ || depth > CS_DEEP)
        return 0;
    if (has_type(s, "null"))
        return 1;
    if (json_get(s, "const", &v) && json_type(v) == J_NULL)
        return 1;
    if (json_get(s, "enum", &v) && json_type(v) == J_ARR) {
        json_iter(v, &it);
        while (json_next(&it, 0, &e))
            if (json_type(e) == J_NULL)
                return 1;
    }
    if (json_get(s, "anyOf", &v) && json_type(v) == J_ARR) {
        json_iter(v, &it);
        while (json_next(&it, 0, &e))
            if (allows_null(e, depth + 1))
                return 1;
    }
    return 0;
}

static int is_required(jv req, jv name)
{
    jit it;
    jv e;
    int r = 0;
    char *nm = json_strdup(name, 0);
    if (!nm)
        return 0;
    if (json_type(req) == J_ARR) {
        json_iter(req, &it);
        while (json_next(&it, 0, &e))
            if (json_type(e) == J_STR && json_streq(e, nm))
                r = 1;
    }
    free(nm);
    return r;
}

static int node(jv s, jw *w, int kw, int depth)
{
    jit it;
    jv k, v, props, req, addl;
    int obj, hasp, hasreq = 0, hasaddl = 0, rc, first = 1;
    req.p = "[]";
    req.n = 2;
    if (json_type(s) != J_OBJ || depth > CS_DEEP)
        return -1;
    /* the checks that come before anything is written */
    json_iter(s, &it);
    while (json_next(&it, &k, &v)) {
        if (in_list(unsupported, k) || (kw == LCS_ANTHROPIC && ant_unsupported(k, v)))
            return -1;
    }
    obj = is_type(s, "object");
    hasp = json_get(s, "properties", &props);
    if (hasp && !obj)
        return -1;
    if (obj) {
        if (json_get(s, "additionalProperties", &addl) && json_type(addl) != J_FALSE)
            return -1;
        if (hasp && json_type(props) != J_OBJ)
            return -1;
        if (json_get(s, "required", &req)) {
            jit ri;
            jv e;
            if (json_type(req) != J_ARR)
                return -1;
            json_iter(req, &ri);
            while (json_next(&ri, 0, &e)) {
                jit pi;
                jv pk, pv;
                int found = 0;
                if (json_type(e) != J_STR)
                    return -1;
                if (hasp) {
                    json_iter(props, &pi);
                    while (json_next(&pi, &pk, &pv)) {
                        char *nm = json_strdup(pk, 0);
                        if (nm && json_streq(e, nm))
                            found = 1;
                        free(nm);
                    }
                }
                if (!found)
                    return -1;
            }
        } else {
            req.p = "[]";
            req.n = 2;
        }
    }
    jw_raw(w, "{", 1);
    json_iter(s, &it);
    while (json_next(&it, &k, &v)) {
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        jw_raw(w, k.p, k.n);
        jw_raw(w, ":", 1);
        if (json_streq(k, "anyOf")) {
            jit ai;
            jv e;
            int n = 0;
            if (json_type(v) != J_ARR || json_count(v) == 0)
                return -1;
            jw_raw(w, "[", 1);
            json_iter(v, &ai);
            while (json_next(&ai, 0, &e)) {
                if (structured(e))
                    return -1;
                if (n++)
                    jw_raw(w, ",", 1);
                if ((rc = node(e, w, kw, depth + 1)) != 0)
                    return rc;
            }
            jw_raw(w, "]", 1);
        } else if (json_streq(k, "items")) {
            if (json_type(v) == J_ARR)
                return -1;
            if ((rc = node(v, w, kw, depth + 1)) != 0)
                return rc;
        } else if (obj && json_streq(k, "properties")) {
            jit pi;
            jv pk, pv;
            int n = 0;
            jw_raw(w, "{", 1);
            json_iter(v, &pi);
            while (json_next(&pi, &pk, &pv)) {
                int wrap = !is_required(req, pk) && !allows_null(pv, 0);
                if (n++)
                    jw_raw(w, ",", 1);
                jw_raw(w, pk.p, pk.n);
                jw_raw(w, ":", 1);
                if (wrap)
                    jw_rawz(w, "{\"anyOf\":[");
                if ((rc = node(pv, w, kw, depth + 1)) != 0)
                    return rc;
                if (wrap)
                    jw_rawz(w, ",{\"type\":\"null\"}]}");
            }
            jw_raw(w, "}", 1);
        } else if (obj && json_streq(k, "required")) {
            jit pi;
            jv pk, pv;
            int n = 0;
            hasreq = 1;
            jw_raw(w, "[", 1);
            if (hasp) {
                json_iter(props, &pi);
                while (json_next(&pi, &pk, &pv)) {
                    if (n++)
                        jw_raw(w, ",", 1);
                    jw_raw(w, pk.p, pk.n);
                }
            }
            jw_raw(w, "]", 1);
        } else if (obj && json_streq(k, "additionalProperties")) {
            hasaddl = 1;
            jw_rawz(w, "false");
        } else
            jw_raw(w, v.p, v.n);
    }
    if (obj) {
        if (!hasreq) {
            jit pi;
            jv pk, pv;
            int n = 0;
            jw_rawz(w, first ? "\"required\":[" : ",\"required\":[");
            first = 0;
            if (hasp) {
                json_iter(props, &pi);
                while (json_next(&pi, &pk, &pv)) {
                    if (n++)
                        jw_raw(w, ",", 1);
                    jw_raw(w, pk.p, pk.n);
                }
            }
            jw_raw(w, "]", 1);
        }
        if (!hasaddl)
            jw_rawz(w, first ? "\"additionalProperties\":false" : ",\"additionalProperties\":false");
    }
    jw_raw(w, "}", 1);
    return w->oom ? -2 : 0;
}

int llm_strict_schema(jv params, jw *out, int kw)
{
    int rc;
    if (json_type(params) != J_OBJ || !is_type(params, "object"))
        return -1;
    rc = node(params, out, kw, 0);
    return rc ? rc : out->oom ? -2 : 0;
}

static void req_err(char *err, long cap, const char *name, const char *why)
{
    cl_copy(err, "Tool \"", cap);
    cl_cat(err, name, cap);
    cl_cat(err, "\" requires JSON-schema constrained sampling, but ", cap);
    cl_cat(err, why, cap);
    cl_cat(err, ".", cap);
}

int llm_tool_strict(jv tool, int supports, int kw, jw *schema, char *err, long errcap)
{
    jv cs, v, params, nm;
    int prefer = 1, rc;
    char name[96];
    if (!json_get(tool, "constrainedSampling", &cs) || json_type(cs) != J_OBJ || !json_get(cs, "type", &v) ||
        !json_streq(v, "json_schema"))
        return 0;
    if (json_get(cs, "strict", &v) && json_streq(v, "require"))
        prefer = 0;
    name[0] = 0;
    if (json_get(tool, "name", &nm))
        json_str(nm, name, sizeof(name));
    if (supports) {
        jw_reset(schema);
        if (!json_get(tool, "parameters", &params))
            params.p = "", params.n = 0;
        rc = params.n ? llm_strict_schema(params, schema, kw) : -1;
        if (rc == -2)
            return -2;
        if (rc == 0)
            return 1;
        if (prefer)
            return 0;
        if (err && errcap > 0)
            req_err(err, errcap, name, "the schema is unsupported");
        return -1;
    }
    if (!prefer) {
        if (err && errcap > 0)
            req_err(err, errcap, name, "strict tools are unsupported");
        return -1;
    }
    return 0;
}
