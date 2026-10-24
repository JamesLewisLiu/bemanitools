#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "gfdmhook1/cabinet.c"
#include "gfdmhook1/profile.c"

static unsigned int writes, reads;
static BOOL WINAPI physical_write(HANDLE fd, LPCVOID data, DWORD size,
                                   LPDWORD count, LPOVERLAPPED ovl)
{
    (void) fd; (void) data; (void) size; (void) count; (void) ovl;
    writes++;
    SetLastError(ERROR_CRC);
    return FALSE;
}
static BOOL WINAPI physical_read(HANDLE fd, LPVOID data, DWORD size,
                                  LPDWORD count, LPOVERLAPPED ovl)
{
    (void) fd; (void) data; (void) size; (void) count; (void) ovl;
    reads++;
    SetLastError(ERROR_CRC);
    return FALSE;
}
static BOOL WINAPI physical_close(HANDLE fd) { (void) fd; return TRUE; }

/* Capture the installed hooks through their public integration boundary. */
void hook_table_apply(HMODULE target, const char *dep,
                       const struct hook_symbol *syms, size_t count)
{
    (void) target; (void) dep; (void) syms; (void) count;
}

static void roundtrip(int black, int enabled, int seq, int flag)
{
    uint8_t req[44] = {43, 0, 0x25, 0};
    uint8_t wire[128], response[128], plain[64];
    struct iobuf out = {wire, sizeof(wire), 0};
    struct iobuf decoded = {plain, sizeof(plain), 0};
    struct const_iobuf in;
    DWORD count, got, total = 0;
    HANDLE fd = (HANDLE) (uintptr_t) (black + 1);
    req[1] = seq;
    req[3] = flag;
    /* Exercise escaping in the native 40-byte request payload. */
    req[4] = 0xAA; req[5] = 0xFF;
    assert(SUCCEEDED(p3io_frame_encode(&out, req, sizeof(req))));
    unsigned int before = writes;
    BOOL ok = cabinet_write(fd, wire, out.pos, &count, NULL);
    if (!enabled) {
        assert(!ok && GetLastError() == ERROR_CRC && writes == before + 1);
        return;
    }
    assert(ok && count == out.pos && writes == before);
    /* Each handle owns a response; reads on another handle remain physical. */
    assert(!cabinet_read((HANDLE) 99, response, sizeof(response), &got, NULL));
    assert(GetLastError() == ERROR_CRC);
    while (true) {
        assert(cabinet_read(fd, response + total, 3, &got, NULL));
        total += got;
        if (total > 1) {
            /* Stop when this particular handle has drained, regardless of slot. */
            bool found = false;
            for (size_t i = 0; i < lengthof(pending); i++) found |= pending[i].fd == fd;
            if (!found) break;
        }
    }
    in.bytes = response; in.nbytes = total; in.pos = 0;
    assert(SUCCEEDED(p3io_frame_decode(&decoded, &in)));
    assert(decoded.pos == 44 && plain[0] == 43 && plain[1] == seq);
    assert(plain[2] == 0x25 && plain[3] == 1);
    assert(memcmp(plain + 4, black ? cabinet_config.pcbid.id : cabinet_config.eamid.id, 8) == 0);
    assert(!cabinet_read(fd, response, sizeof(response), &got, NULL));
    assert(GetLastError() == ERROR_CRC);
}

int main(void)
{
    struct gfdmhook1_config settings = {0};
    settings.pcbid = security_id_default;
    settings.eamid = security_id_default;
    next_write = physical_write; next_read = physical_read; next_close = physical_close;
    gfdm_cabinet_init(&settings, gfdm_profile_find("gdv4.exe"));
    for (unsigned int version = 4; version <= 8; version++) {
        char exe[16]; snprintf(exe, sizeof(exe), "gdv%u.exe", version);
        cabinet_profile = gfdm_profile_find(exe);
        for (unsigned int mode = 0; mode < 2; mode++) {
        assert(security_mcode_parse(mode ? cabinet_profile->mcode_dm : cabinet_profile->mcode_gf, &cabinet_config.mcode));
        for (unsigned int mask = 0; mask < 4; mask++) {
            cabinet_config.black = (mask & 1) != 0;
            cabinet_config.white = (mask & 2) != 0;
            for (unsigned int seq = 0; seq < 16; seq++) {
                roundtrip(1, cabinet_config.black, seq, 0x10);
                roundtrip(1, cabinet_config.black, seq, 0x12);
                roundtrip(0, cabinet_config.white, seq, 0);
                roundtrip(0, cabinet_config.white, seq, 2);
            }
        }
        }
    }
    /* Non-plug packets, malformed frames and asynchronous calls are physical. */
    uint8_t wire[] = {0xAA, 3, 1, 0x24, 0};
    DWORD count;
    assert(!cabinet_write((HANDLE) 1, wire, sizeof(wire), &count, NULL));
    assert(GetLastError() == ERROR_CRC);
    wire[0] = 0;
    assert(!cabinet_write((HANDLE) 1, wire, sizeof(wire), &count, NULL));
    assert(GetLastError() == ERROR_CRC);
    /* Closing a handle clears a queued reply before Windows can reuse it. */
    pending[0].fd = (HANDLE) 1; pending[0].size = 10;
    assert(cabinet_close((HANDLE) 1));
    assert(pending[0].fd == NULL);
    /* Eight outstanding independent handles; a ninth cannot corrupt a reply. */
    uint8_t req[44] = {43, 3, 0x25, 0x10}, encoded[128];
    struct iobuf encoded_out = {encoded, sizeof(encoded), 0};
    assert(SUCCEEDED(p3io_frame_encode(&encoded_out, req, sizeof(req))));
    for (uintptr_t i = 1; i <= 8; i++) {
        assert(cabinet_write((HANDLE) i, encoded, encoded_out.pos, &count, NULL));
    }
    assert(!cabinet_write((HANDLE) 9, encoded, encoded_out.pos, &count, NULL));
    assert(GetLastError() == ERROR_NOT_ENOUGH_MEMORY);
    assert(!cabinet_write((HANDLE) 1, encoded, encoded_out.pos, &count, NULL));
    assert(GetLastError() == ERROR_BUSY);
    for (uintptr_t i = 1; i <= 8; i++) assert(cabinet_close((HANDLE) i));
    puts("PASS cabinet: five versions, four plug modes, all sequences, ROM/EEPROM, escaping, partial reads, physical error forwarding, close/reuse");
    return 0;
}
