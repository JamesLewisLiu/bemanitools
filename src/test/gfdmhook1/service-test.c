#undef NDEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "gfdmhook1/config.c"
#include "gfdmhook1/network.c"

static char current_url[64];
static int root_token, node_token;
static unsigned int installs, removes, creates, calls;

void hook_table_apply(HMODULE target, const char *dep,
                       const struct hook_symbol *syms, size_t count)
{
    assert(target == NULL && count == 1);
    assert(!strcmp(dep, "libavs-win32.dll") || !strcmp(dep, "libavs-win32-ea3.dll"));
    assert(!strcmp(syms[0].name, "ea3_boot"));
    installs++;
}
struct property_node *property_search(struct property *p,
    struct property_node *root, const char *path)
{
    assert(p == NULL && root == (void *) &root_token);
    assert(!strcmp(path, "network/services"));
    return current_url[0] ? (void *) &node_token : NULL;
}
void property_node_remove(struct property_node *node)
{
    assert(node == (void *) &node_token); removes++; current_url[0] = 0;
}
struct property_node *property_node_create(struct property *p,
    struct property_node *root, int type, const char *key, ...)
{
    va_list args;
    assert(p == NULL && root == (void *) &root_token && type == PROPERTY_TYPE_STR);
    assert(!strcmp(key, "network/services"));
    va_start(args, key); strcpy(current_url, va_arg(args, const char *)); va_end(args);
    creates++;
    return (void *) &node_token;
}
int property_node_datasize(struct property_node *node)
{
    assert(node == (void *) &node_token); return (int) strlen(current_url) + 1;
}
static int __cdecl original_boot(struct property_node *root)
{
    assert(root == (void *) &root_token && !strcmp(current_url, service_url));
    calls++;
    return -123; /* Preserve the native return value. */
}
int main(void)
{
    const char *input[] = {"localhost:80", "192.0.2.1:8080",
                           "http://example.test:8080/gfdm/services",
                           "https://example.test/services"};
    const char *expected[] = {"http://localhost:80/", "http://192.0.2.1:8080/",
                              "http://example.test:8080/gfdm/services",
                              "https://example.test/services"};
    for (size_t i = 0; i < lengthof(input); i++) {
        struct cconfig *config = cconfig_init();
        struct gfdmhook1_config settings;
        gfdmhook1_config_init(config, "GCG33JAA");
        cconfig_set2(config, "eamuse.server", input[i]);
        gfdmhook1_config_get(&settings, config, "GCG33JAA");
        assert(!settings.black && !settings.white);
        assert(!strcmp(settings.server, expected[i]));
        for (size_t mask = 0; mask < 4; mask++) {
            cconfig_set2(config, "security.black", mask & 1 ? "true" : "false");
            cconfig_set2(config, "security.white", mask & 2 ? "true" : "false");
            gfdmhook1_config_get(&settings, config, "GCG33JAA");
            assert(settings.black == ((mask & 1) != 0));
            assert(settings.white == ((mask & 2) != 0));
        }
        gfdmhook1_network_init(settings.server);
        next_ea3_boot = original_boot;
        assert(cabinet_ea3_boot((void *) &root_token) == -123);
        assert(cabinet_ea3_boot((void *) &root_token) == -123); /* TEST reboot. */
        cconfig_finit(config);
    }
    assert(calls == 8 && creates == 8 && removes == 7 && installs == 8);
    puts("PASS service: full URL/path/port, HTTPS text preservation, default physical plugs, independent switches, both EA3 DLLs, repeated boot and native return value");
    return 0;
}
