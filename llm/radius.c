/* radius -- see radius.h. */
#include <stdlib.h>
#include <string.h>
#include "radius.h"
#include "../claude/util.h"

void llm_radius_gateway(const char *value, int strip_v1, char *out, long cap)
{
    char tmp[400];
    long n;
    cl_copy(tmp, value ? value : "", sizeof(tmp));
    n = (long)strlen(tmp);
    if (strip_v1) {
        /* replace(/\/v1\/?$/u, "") */
        if (n >= 4 && !strcmp(tmp + n - 4, "/v1/"))
            tmp[n - 4] = 0;
        else if (n >= 3 && !strcmp(tmp + n - 3, "/v1"))
            tmp[n - 3] = 0;
    }
    if (!(cl_strnieq(tmp, "http://", 7) || cl_strnieq(tmp, "https://", 8))) {
        char t2[400];
        cl_copy(t2, "https://", sizeof(t2));
        cl_cat(t2, tmp, sizeof(t2));
        cl_copy(tmp, t2, sizeof(tmp));
    }
    n = (long)strlen(tmp);
    while (n > 0 && tmp[n - 1] == '/')
        tmp[--n] = 0;
    cl_copy(out, tmp, cap);
}

/* isRadiusGatewayModel */
static int valid_model(jv m)
{
    jv v;
    if (json_type(m) != J_OBJ)
        return 0;
    return json_get(m, "id", &v) && json_type(v) == J_STR && json_get(m, "name", &v) && json_type(v) == J_STR &&
           json_get(m, "reasoning", &v) && (json_type(v) == J_TRUE || json_type(v) == J_FALSE) &&
           json_get(m, "input", &v) && json_type(v) == J_ARR && json_get(m, "cost", &v) && json_type(v) == J_OBJ &&
           json_get(m, "contextWindow", &v) && json_type(v) == J_NUM && json_get(m, "maxTokens", &v) &&
           json_type(v) == J_NUM;
}

int llm_radius_models(const char *provider, const char *config, long n, jw *models)
{
    jv root, base, list, m, k, v;
    jit it, mi;
    int count = 0;
    char *burl;
    if (json_parse(config, n, &root) || json_type(root) != J_OBJ || !json_get(root, "baseUrl", &base) ||
        json_type(base) != J_STR || !json_get(root, "models", &list) || json_type(list) != J_ARR)
        return -1;
    burl = json_strdup(base, 0);
    if (!burl)
        return -1;
    jw_rawz(models, "[");
    json_iter(list, &it);
    while (json_next(&it, 0, &m)) {
        int first = 1;
        if (!valid_model(m))
            continue;
        jw_rawz(models, count++ ? ",{" : "{");
        json_iter(m, &mi);
        while (json_next(&mi, &k, &v)) {
            if (json_streq(k, "api") || json_streq(k, "provider") || json_streq(k, "baseUrl"))
                continue;
            if (!first)
                jw_rawz(models, ",");
            first = 0;
            jw_raw(models, k.p, k.n);
            jw_rawz(models, ":");
            jw_raw(models, v.p, v.n);
        }
        jw_rawz(models, ",\"api\":\"pi-messages\",\"provider\":");
        jw_strz(models, provider);
        jw_rawz(models, ",\"baseUrl\":");
        jw_strz(models, burl);
        jw_rawz(models, "}");
    }
    jw_rawz(models, "]");
    free(burl);
    return models->oom ? -1 : 0;
}

int llm_radius_apply(llm_reg *r, const char *provider, const char *models, long n, char *err, long cap)
{
    jv list, m, v;
    jit it;
    jw text;
    llm_model *save = 0;
    int save_n = r->n, count = 0, rc = 0;
    if (json_parse(models, n, &list) || json_type(list) != J_ARR) {
        cl_copy(err, "the Radius model list is not an array", cap);
        return -1;
    }
    jw_init(&text);
    jw_rawz(&text, "{\"providers\":{");
    jw_strz(&text, provider);
    jw_rawz(&text, ":{\"api\":\"pi-messages\",\"models\":[");
    json_iter(list, &it);
    while (json_next(&it, 0, &m)) {
        if (json_type(m) != J_OBJ || !json_get(m, "provider", &v) || !json_streq(v, provider))
            continue;           /* models.filter((model) => model.provider === id) */
        if (count++)
            jw_rawz(&text, ",");
        jw_raw(&text, m.p, m.n);
    }
    jw_rawz(&text, "]}}}");
    if (text.oom) {
        jw_free(&text);
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    if (r->n) {
        save = (llm_model *)malloc((size_t)r->n * sizeof(*save));
        if (!save) {
            jw_free(&text);
            cl_copy(err, "out of memory", cap);
            return -1;
        }
        memcpy(save, r->m, (size_t)r->n * sizeof(*save));
    }
    llm_reg_drop_provider(r, provider);
    if (count && llm_reg_overlay(r, text.p, text.n)) {
        cl_copy(err, r->err, cap);
        if (save)
            memcpy(r->m, save, (size_t)save_n * sizeof(*save));
        r->n = save_n;
        rc = -1;
    }
    free(save);
    jw_free(&text);
    return rc;
}
