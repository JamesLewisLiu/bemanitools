#pragma once
#include <stdbool.h>
#include "cconfig/cconfig.h"
#include "security/id.h"
#include "security/mcode.h"

struct gfdmhook1_config {
    char server[64]; /* V4 EA3 services field includes the terminator. */
    struct security_id pcbid;
    struct security_id eamid;
    struct security_mcode mcode;
    bool black;
    bool white;
};
void gfdmhook1_config_init(struct cconfig *config, const char *default_mcode);
void gfdmhook1_config_get(struct gfdmhook1_config *out, struct cconfig *config,
                         const char *default_mcode);
