#include <stdbool.h>
#include <string.h>

#include "gfdmhook1/cabinet.h"
#include "hook/table.h"
#include "p3io/cmd.h"
#include "security/rp3.h"
#include "util/defs.h"
#include "util/log.h"

/* Only the synchronous bulk transport imports in the native libdevice are
   intercepted. SetupAPI, device open, interrupt IOCTLs and UART stay native.
   Native libdevice serializes each bulk write/read transaction with a mutex. */
static struct gfdmhook1_config cabinet_config;
static const struct gfdm_profile *cabinet_profile;
static CRITICAL_SECTION cabinet_lock;
static struct {
    HANDLE fd;
    uint8_t bytes[128];
    size_t size;
    size_t pos;
} pending[8];

static BOOL (WINAPI *next_write)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL (WINAPI *next_read)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL (WINAPI *next_close)(HANDLE);

static BOOL WINAPI cabinet_write(HANDLE fd, LPCVOID data, DWORD size,
                                 LPDWORD written, LPOVERLAPPED ovl)
{
    uint8_t decoded[64];
    struct iobuf dest = {decoded, sizeof(decoded), 0};
    struct const_iobuf src = {data, size, 0};
    const struct p3io_req_read_plug *req = (const void *) decoded;
    struct p3io_resp_read_plug resp;
    struct security_rp3_eeprom eeprom;
    bool black;
    size_t slot;

    if (ovl != NULL || data == NULL || written == NULL ||
        FAILED(p3io_frame_decode(&dest, &src)) ||
        dest.pos != 44 || req->hdr.nbytes != 43 ||
        req->hdr.cmd != P3IO_CMD_READ_PLUG ||
        (req->flags != 0 && req->flags != 2 &&
         req->flags != 0x10 && req->flags != 0x12)) {
        return next_write(fd, data, size, written, ovl);
    }
    black = (req->flags & 0x10) != 0;
    if (!(black ? cabinet_config.black : cabinet_config.white)) {
        return next_write(fd, data, size, written, ovl);
    }

    memset(&resp, 0, sizeof(resp));
    p3io_resp_hdr_init(&resp.hdr, sizeof(resp), &req->hdr);
    resp.present = 1;
    memcpy(resp.rom, black ? cabinet_config.pcbid.id : cabinet_config.eamid.id, 8);
    security_rp3_generate_signed_eeprom_data(
        black ? SECURITY_RP_UTIL_RP_TYPE_BLACK : SECURITY_RP_UTIL_RP_TYPE_WHITE,
        black ? cabinet_profile->black_sign_key : &security_rp_sign_key_white_eamuse,
        black ? &cabinet_config.mcode : &security_mcode_eamuse,
        black ? &cabinet_config.pcbid : &cabinet_config.eamid, &eeprom);
    memcpy(resp.eeprom, &eeprom, sizeof(eeprom));

    EnterCriticalSection(&cabinet_lock);
    for (slot = 0; slot < lengthof(pending); slot++) {
        if (pending[slot].fd == fd) {
            LeaveCriticalSection(&cabinet_lock);
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
    }
    for (slot = 0; slot < lengthof(pending); slot++) {
        if (pending[slot].fd == NULL) break;
    }
    if (slot == lengthof(pending)) {
        LeaveCriticalSection(&cabinet_lock);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    dest.bytes = pending[slot].bytes;
    dest.nbytes = sizeof(pending[slot].bytes);
    dest.pos = 0;
    if (FAILED(p3io_frame_encode(&dest, &resp, sizeof(resp)))) {
        LeaveCriticalSection(&cabinet_lock);
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    pending[slot].fd = fd;
    pending[slot].size = dest.pos;
    pending[slot].pos = 0;
    LeaveCriticalSection(&cabinet_lock);
    *written = size;
    return TRUE;
}

static BOOL WINAPI cabinet_read(HANDLE fd, LPVOID data, DWORD size,
                                LPDWORD received, LPOVERLAPPED ovl)
{
    size_t slot, count;
    if (ovl != NULL || data == NULL || received == NULL) {
        return next_read(fd, data, size, received, ovl);
    }
    EnterCriticalSection(&cabinet_lock);
    for (slot = 0; slot < lengthof(pending); slot++) {
        if (pending[slot].fd == fd) {
            count = pending[slot].size - pending[slot].pos;
            if (count > size) count = size;
            memcpy(data, pending[slot].bytes + pending[slot].pos, count);
            pending[slot].pos += count;
            if (pending[slot].pos == pending[slot].size) {
                memset(&pending[slot], 0, sizeof(pending[slot]));
            }
            LeaveCriticalSection(&cabinet_lock);
            *received = (DWORD) count;
            return TRUE;
        }
    }
    LeaveCriticalSection(&cabinet_lock);
    return next_read(fd, data, size, received, ovl);
}

static BOOL WINAPI cabinet_close(HANDLE fd)
{
    size_t slot;
    BOOL result = next_close(fd);
    if (result) {
        EnterCriticalSection(&cabinet_lock);
        for (slot = 0; slot < lengthof(pending); slot++) {
            if (pending[slot].fd == fd) memset(&pending[slot], 0, sizeof(pending[slot]));
        }
        LeaveCriticalSection(&cabinet_lock);
    }
    return result;
}

static const struct hook_symbol cabinet_syms[] = {
    {.name = "WriteFile", .patch = cabinet_write, .link = (void **) &next_write},
    {.name = "ReadFile", .patch = cabinet_read, .link = (void **) &next_read},
    {.name = "CloseHandle", .patch = cabinet_close, .link = (void **) &next_close},
};

void gfdm_cabinet_init(const struct gfdmhook1_config *config,
                       const struct gfdm_profile *profile)
{
    cabinet_config = *config;
    cabinet_profile = profile;
    InitializeCriticalSection(&cabinet_lock);
    log_info("Native cabinet: black plug %s, white plug %s",
             config->black ? "emulated" : "physical",
             config->white ? "emulated" : "physical");
}

void gfdm_cabinet_apply_hooks(void)
{
    HMODULE device;
    if (!cabinet_config.black && !cabinet_config.white) return;
    device = GetModuleHandleA("libdevice.dll");
    if (device != NULL) {
        hook_table_apply(device, "kernel32.dll", cabinet_syms, lengthof(cabinet_syms));
    }
}
