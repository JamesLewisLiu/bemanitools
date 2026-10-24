#include <windows.h>
#include <string.h>
#include "gfdmhook1/network.h"
#include "hook/table.h"
#include "imports/avs.h"
#include "util/defs.h"
#include "util/log.h"

static char service_url[64];
static int (__cdecl *next_ea3_boot)(struct property_node *);

static int __cdecl cabinet_ea3_boot(struct property_node *root)
{
    struct property_node *node = property_search(NULL, root, "network/services");
    if (node != NULL) property_node_remove(node);
    node = property_node_create(NULL, root, PROPERTY_TYPE_STR,
                                "network/services", service_url);
    if (node == NULL) log_fatal("Cannot replace EA3 Service URL");
    property_node_datasize(node);
    log_info("EA3 Service URL: %s", service_url);
    return next_ea3_boot(root);
}
static const struct hook_symbol service_syms[] = {
    {.name = "ea3_boot", .patch = cabinet_ea3_boot, .link = (void **) &next_ea3_boot},
};
void gfdmhook1_network_apply_hooks(void)
{
    hook_table_apply(NULL, "libavs-win32.dll", service_syms, lengthof(service_syms));
    hook_table_apply(NULL, "libavs-win32-ea3.dll", service_syms, lengthof(service_syms));
}
void gfdmhook1_network_init(const char *server)
{
    strcpy(service_url, server);
    gfdmhook1_network_apply_hooks();
}
