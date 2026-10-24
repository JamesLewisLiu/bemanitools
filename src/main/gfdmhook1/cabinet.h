#pragma once

#include <windows.h>
#include "gfdmhook1/config.h"
#include "gfdmhook1/profile.h"

void gfdm_cabinet_init(const struct gfdmhook1_config *config,
                       const struct gfdm_profile *profile);
void gfdm_cabinet_apply_hooks(void);
