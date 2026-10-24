#include <string.h>
#include <stdio.h>

#include "cconfig/cconfig-util.h"
#include "gfdmhook1/config.h"
#include "security/id.h"
#include "util/log.h"

#define SERVER_KEY "eamuse.server"
#define PCBID_KEY "eamuse.pcbid"
#define EAMID_KEY "eamuse.eamid"
#define MCODE_KEY "security.mcode"

void gfdmhook1_config_init(struct cconfig *config, const char *default_mcode)
{
    cconfig_util_set_str(
        config,
        SERVER_KEY,
        "localhost:80",
        "e-amusement server URL or host:port");

    cconfig_util_set_data(
        config,
        PCBID_KEY,
        (const uint8_t *) &security_id_default,
        sizeof(security_id_default),
        "system/network PCBID and black roundplug ID");
    cconfig_util_set_data(
        config,
        EAMID_KEY,
        (const uint8_t *) &security_id_default,
        sizeof(security_id_default),
        "white roundplug EAMID");
    cconfig_util_set_str(
        config,
        MCODE_KEY,
        default_mcode,
        "security mcode (8 characters; default follows version and GF/DM mode)");
    cconfig_util_set_bool(config, "security.black", false,
                          "emulate black roundplug; false uses physical plug");
    cconfig_util_set_bool(config, "security.white", false,
                          "emulate white roundplug; false uses physical plug");
}

void gfdmhook1_config_get(
    struct gfdmhook1_config *out, struct cconfig *config,
    const char *default_mcode)
{
    char server[1025];
    char mcode[sizeof(struct security_mcode) + 2];

    memset(out, 0, sizeof(*out));
    out->pcbid = security_id_default;
    out->eamid = security_id_default;
    if (!cconfig_util_get_str(config, SERVER_KEY, server, sizeof(server) - 1, "localhost:80")) {
        log_fatal("Cannot read Service URL");
    }
    if (strncmp(server, "http://", 7) && strncmp(server, "https://", 8)) {
        if (strstr(server, "://") || strlen(server) + 8 >= sizeof(out->server)) {
            log_fatal("Invalid or too long Service URL");
        }
        snprintf(out->server, sizeof(out->server), "http://%s/", server);
    } else {
        if (strlen(server) >= sizeof(out->server)) log_fatal("Service URL exceeds EA3 limit");
        strcpy(out->server, server);
    }
    const char *host = strstr(out->server, "://") + 3;
    if (*host == '\0' || *host == '/' || strpbrk(host, " \t\r\n")) {
        log_fatal("Invalid Service URL host");
    }
    cconfig_util_get_bool(config, "security.black", &out->black, false);
    cconfig_util_get_bool(config, "security.white", &out->white, false);

    cconfig_util_get_data(
        config, PCBID_KEY, (uint8_t *) &out->pcbid, sizeof(out->pcbid),
        (const uint8_t *) &security_id_default);
    cconfig_util_get_data(
        config, EAMID_KEY, (uint8_t *) &out->eamid, sizeof(out->eamid),
        (const uint8_t *) &security_id_default);

    if (!security_id_verify(&out->pcbid)) {
        out->pcbid = security_id_default;
    }
    if (!security_id_verify(&out->eamid)) {
        out->eamid = security_id_default;
    }

    if (!cconfig_util_get_str(
            config, MCODE_KEY, mcode, sizeof(mcode) - 1, default_mcode) ||
        !security_mcode_parse(mcode, &out->mcode)) {
        log_warning("Invalid %s, using %s", MCODE_KEY, default_mcode);
        security_mcode_parse(default_mcode, &out->mcode);
    }

}
