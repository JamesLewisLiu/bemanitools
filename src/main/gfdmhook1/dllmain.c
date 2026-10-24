#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cconfig/cconfig-hook.h"
#include "gfdmhook1/cabinet.h"
#include "gfdmhook1/network.h"
#include "hook/table.h"
#include "util/defs.h"
#include "util/log.h"
#include "util/str.h"

static HMODULE gfdm_module;
static FILE *gfdm_log_file;
static const struct gfdm_profile *gfdm_profile;
static bool initialized;
static HMODULE (WINAPI *next_load)(LPCSTR);
static void (__cdecl *next_boot)(HWND);

static bool gfdm_command_has_switch(const char *cmdline, char wanted)
{
    const char *p;

    for (p = cmdline; p != NULL && *p != '\0';) {
        char token[16];
        size_t n;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        n = 0;
        if (*p == '"') {
            p++;
            while (*p != '\0' && *p != '"') {
                if (n + 1 < sizeof(token)) {
                    token[n++] = *p;
                }
                p++;
            }
            if (*p == '"') {
                p++;
            }
        } else {
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                if (n + 1 < sizeof(token)) {
                    token[n++] = *p;
                }
                p++;
            }
        }

        token[n] = '\0';
        if ((n == 2 && token[0] == '-' && token[1] == wanted) ||
            (n == 2 && token[0] == '/' && token[1] == wanted)) {
            return true;
        }
    }

    return false;
}

static void gfdm_open_log_file(void)
{
    char module_path[MAX_PATH];
    char *separator;
    char log_path[MAX_PATH];

    if (gfdm_log_file != NULL) {
        return;
    }

    if (GetModuleFileNameA(gfdm_module, module_path,
                           lengthof(module_path)) > 0 &&
        (separator = strrchr(module_path, '\\')) != NULL) {
        separator[1] = '\0';
        str_format(log_path, sizeof(log_path),
                   "%sgfdm-v%u-hook.log", module_path, gfdm_profile->version);
        gfdm_log_file = fopen(log_path, "a");
    }

    if (gfdm_log_file == NULL) {
        str_format(log_path, sizeof(log_path), "gfdm-v%u-hook.log",
                   gfdm_profile->version);
        gfdm_log_file = fopen(log_path, "a");
    }
}

static void gfdm_log_writer(void *ctx, const char *chars, size_t nchars)
{
    (void) ctx;

    OutputDebugStringA(chars);

    if (gfdm_log_file != NULL) {
        fwrite(chars, 1, nchars, gfdm_log_file);
        fflush(gfdm_log_file);
    }
}


static HMODULE WINAPI cabinet_load(LPCSTR name)
{
    HMODULE module = next_load(name);
    if (module != NULL) {
        gfdm_cabinet_apply_hooks();
        gfdmhook1_network_apply_hooks();
    }
    return module;
}

static const struct hook_symbol loader_syms[] = {
    {.name = "LoadLibraryA", .patch = cabinet_load, .link = (void **) &next_load},
};

static void __cdecl cabinet_boot(HWND window)
{
    struct cconfig *config;
    struct gfdmhook1_config settings;
    const char *mcode;
    if (!initialized) {
        initialized = true;
        mcode = gfdm_command_has_switch(GetCommandLineA(), 'g')
            ? gfdm_profile->mcode_gf : gfdm_profile->mcode_dm;
        config = cconfig_init();
        gfdmhook1_config_init(config, mcode);
        if (!cconfig_hook_config_init(config,
                "GFDM V4-V8 original cabinet hook", CCONFIG_CMD_USAGE_OUT_DBG)) {
            cconfig_finit(config);
            ExitProcess(EXIT_FAILURE);
        }
        gfdmhook1_config_get(&settings, config, mcode);
        cconfig_finit(config);
        gfdm_cabinet_init(&settings, gfdm_profile);
        gfdmhook1_network_init(settings.server);
        hook_table_apply(NULL, "kernel32.dll", loader_syms, lengthof(loader_syms));
        gfdm_cabinet_apply_hooks();
        log_info("GFDM V%u native IO ready; original storage/audio/video retained",
                 gfdm_profile->version);
    }
    next_boot(window);
}

static const struct hook_symbol boot_syms[] = {
    {.name = "?boot_main@@YAXPAUHWND__@@@Z", .patch = cabinet_boot,
     .link = (void **) &next_boot},
};

BOOL WINAPI DllMain(HMODULE mod, DWORD reason, void *context)
{
    char executable[MAX_PATH];
    DWORD size;
    (void) context;
    if (reason == DLL_PROCESS_ATTACH) {
        size = GetModuleFileNameA(NULL, executable, sizeof(executable));
        if (size == 0 || size >= sizeof(executable)) return FALSE;
        gfdm_profile = gfdm_profile_find(executable);
        if (gfdm_profile == NULL) return FALSE;
        gfdm_module = mod;
        gfdm_open_log_file();
        log_to_writer(gfdm_log_writer, NULL);
        hook_table_apply(NULL, "boot.dll", boot_syms, lengthof(boot_syms));
    }
    return TRUE;
}
