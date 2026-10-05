/* schema -- see schema.h. */
#include <string.h>
#include "schema.h"
#include "util.h"

static const char *type_name(int t)
{
    switch (t) {
    case J_OBJ:
        return "an object";
    case J_ARR:
        return "an array";
    case J_STR:
        return "a string";
    case J_NUM:
        return "a number";
    case J_TRUE:
    case J_FALSE:
        return "a boolean";
    case J_NULL:
        return "null";
    }
    return "invalid";
}

static int is_integer(jv v)
{
    long i;
    for (i = 0; i < v.n; i++)
        if (v.p[i] == '.' || v.p[i] == 'e' || v.p[i] == 'E')
            return 0;
    return 1;
}

static int type_ok(jv want, jv v)
{
    int t = json_type(v);
    if (json_streq(want, "string"))
        return t == J_STR;
    if (json_streq(want, "number"))
        return t == J_NUM;
    if (json_streq(want, "integer"))
        return t == J_NUM && is_integer(v);
    if (json_streq(want, "boolean"))
        return t == J_TRUE || t == J_FALSE;
    if (json_streq(want, "array"))
        return t == J_ARR;
    if (json_streq(want, "object"))
        return t == J_OBJ;
    if (json_streq(want, "null"))
        return t == J_NULL;
    return 1;
}

static void say(char *err, long cap, const char *path, const char *a, const char *b, const char *c)
{
    cl_copy(err, a, cap);
    if (path && *path) {
        cl_cat(err, "`", cap);
        cl_cat(err, path, cap);
        cl_cat(err, "`", cap);
    }
    if (b)
        cl_cat(err, b, cap);
    if (c)
        cl_cat(err, c, cap);
}

static int check(jv s, jv v, char *path, long pcap, char *err, long cap, int depth)
{
    jv t, x;
    long pl = (long)strlen(path);
    if (depth > 8)
        return 0;
    if (json_get(s, "type", &t) && json_type(t) == J_STR && !type_ok(t, v)) {
        char want[16];
        json_str(t, want, sizeof(want));
        say(err, cap, path, "The parameter ", " must be ", strcmp(want, "integer") ? "" : "a whole number");
        if (strcmp(want, "integer")) {
            cl_cat(err, !strcmp(want, "object") || !strcmp(want, "array") ? "an " : "a ", cap);
            cl_cat(err, want, cap);
        }
        cl_cat(err, ", not ", cap);
        cl_cat(err, type_name(json_type(v)), cap);
        return -1;
    }
    if (json_get(s, "enum", &x) && json_type(x) == J_ARR) {
        jit it;
        jv e;
        int ok = 0;
        json_iter(x, &it);
        while (json_next(&it, 0, &e))
            if (e.n == v.n && !memcmp(e.p, v.p, (size_t)v.n))
                ok = 1;
        if (!ok) {
            say(err, cap, path, "The parameter ", " must be one of ", 0);
            {
                char list[200];
                long k = x.n < (long)sizeof(list) - 1 ? x.n : (long)sizeof(list) - 1;
                memcpy(list, x.p, (size_t)k);
                list[k] = 0;
                cl_cat(err, list, cap);
            }
            return -1;
        }
    }
    if (json_type(v) == J_OBJ) {
        jv props, req, add, k, val;
        jit it;
        int have_props = json_get(s, "properties", &props) && json_type(props) == J_OBJ;
        int closed = json_get(s, "additionalProperties", &add) && json_type(add) == J_FALSE;
        if (json_get(s, "required", &req) && json_type(req) == J_ARR) {
            jit r;
            jv name;
            json_iter(req, &r);
            while (json_next(&r, 0, &name)) {
                char n[64];
                json_str(name, n, sizeof(n));
                if (!json_get(v, n, &val)) {
                    if (pl) {
                        cl_cat(path, ".", pcap);
                    }
                    cl_cat(path, n, pcap);
                    say(err, cap, path, "The required parameter ", " is missing", 0);
                    path[pl] = 0;
                    return -1;
                }
            }
        }
        json_iter(v, &it);
        while (json_next(&it, &k, &val)) {
            char n[64];
            jv ps;
            json_str(k, n, sizeof(n));
            if (pl)
                cl_cat(path, ".", pcap);
            cl_cat(path, n, pcap);
            if (have_props && json_get(props, n, &ps)) {
                if (check(ps, val, path, pcap, err, cap, depth + 1))
                    return -1;
            } else if (closed) {
                say(err, cap, path, "An unexpected parameter ", " was provided", 0);
                return -1;
            }
            path[pl] = 0;
        }
    } else if (json_type(v) == J_ARR && json_get(s, "items", &x) && json_type(x) == J_OBJ) {
        jit it;
        jv e;
        long i = 0;
        json_iter(v, &it);
        while (json_next(&it, 0, &e)) {
            char num[16];
            cl_ltoa(i++, num);
            cl_cat(path, "[", pcap);
            cl_cat(path, num, pcap);
            cl_cat(path, "]", pcap);
            if (check(x, e, path, pcap, err, cap, depth + 1))
                return -1;
            path[pl] = 0;
        }
    }
    return 0;
}

int schema_check(jv schema, jv value, char *err, long cap)
{
    char path[160];
    path[0] = 0;
    err[0] = 0;
    return check(schema, value, path, sizeof(path), err, cap, 0);
}
