# GFDM V4–V8 original cabinets

Branch `gfdm-original-hardware` starts at `gfdm` commit
`11ddee0b9953c5f4bef14dfe5214e6695abb0cd7`. It requires original P3IO hardware,
drivers, readers and game libraries. The `gfdm` branch retains PC IO emulation.

Back up existing tools, then copy this package beside the matching `gdvN.exe`.
Start with `gamestart-vN-gf.bat` or `gamestart-vN-dm.bat`. Launchers preserve
original boot XML and EA3 identity selection; additional original arguments
including explicit `-b`/`-I` can be supplied to the BAT if the cabinet needs them.
Storage, saves, graphics timing, audio, codecs, network adapters, BIOS and identity
publication remain native. Do not install the PC branch's replacement boot/identity
XML or input DLLs over the cabinet. The mapper UI is not shipped in this branch.

Edit the selected mode's `gfdm-vN-gf.conf` or `gfdm-vN-dm.conf`:

```ini
eamuse.server=http://192.0.2.1:8080/gfdm/services
security.black=false
security.white=false
```

`eamuse.server` replaces EA3 `network/services` before every native `ea3_boot`.
Full HTTP/HTTPS URLs preserve scheme, port and path. `host:port` expands to
`http://host:port/`. The URL must fit the common 63-byte EA3 field. Overlong or
invalid values stop startup. HTTPS depends on native AVS TLS support. There is
no DNS/socket redirection; discovery responses must advertise reachable URLs.

| security.black | security.white | Plugs |
| --- | --- | --- |
| false | false | Both physical; no P3IO bulk hooks installed |
| true | false | Emulated black, physical white |
| false | true | Physical black, emulated white |
| true | true | Both emulated; all other IO remains physical |

Both switches default to false and require restarting the game after edits.
Black uses `eamuse.pcbid` and `security.mcode`; white uses `eamuse.eamid`.
These configured values are ignored for physical plugs. Native libdevice derives
system/network IDs from ROM bytes using its own formatting; this branch does
not force `/env/profile/sysid` to the configured PCBID.

Only synchronous 0x25 ROM/EEPROM transactions for an enabled plug are answered
inside the original libdevice bulk transport. Native signature checks, caches
and security exports remain unchanged. Black signing keys are GENTAKAH for V4,
UDONHRKI for V5–V7 and SHAMOSAN for V8; white uses E-AMUSE3. Replies preserve sequence and
framing. Other requests and physical failures are forwarded unchanged.
SetupAPI, device open, interrupt JAMMA reads, lamps, coins, DIP switches,
UART/readers and keypads stay native. A physical P3IO is always required.

Official IDA verified the supplied 32-bit V4–V8 bulk transports and EA3 entry
points. V4–V6 import EA3 from `libavs-win32.dll`; V7/V8 use
`libavs-win32-ea3.dll`. No device-initialize prototypes or offsets are patched.
Other revisions require verification.

Local x86/x64 tests cover switches, framing, forwarding, partial reads,
close/reuse and service replacement. Original cabinet startup, physical IO and
readers, four plug combinations, online binary XML and full songs still need
validation on each machine. Host tests do not establish cabinet compatibility.
