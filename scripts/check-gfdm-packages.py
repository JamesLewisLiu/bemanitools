"""Validate original-cabinet packages."""
import pathlib,sys,zipfile
def check(directory):
    common = None
    for v in range(4,9):
        prefix=f"gfdm-v{v}"
        with zipfile.ZipFile(directory/f"{prefix}.zip") as z:
            expected={"gfdmhook1.dll","inject.exe","README-original-cabinet.md",f"{prefix}.conf"}
            for mode in ("gf","dm"):
                expected.update({f"gamestart-v{v}-{mode}.bat",f"{prefix}-{mode}.conf"})
            assert len(z.namelist())==len(expected) and set(z.namelist())==expected
            dll=z.read("gfdmhook1.dll")
            assert dll[:2]==b"MZ" and (common is None or dll==common)
            common=dll
            for mode,switch in [("gf","g"),("dm","d")]:
                launch=z.read(f"gamestart-v{v}-{mode}.bat").decode()
                assert f"gdv{v}.exe -{switch} --config {prefix}-{mode}.conf" in launch
                assert " -I " not in launch and " -b " not in launch and "mkdir" not in launch
                conf=dict(line.split("=",1) for line in z.read(f"{prefix}-{mode}.conf").decode().splitlines() if line and not line.startswith("#"))
                assert conf["security.black"]==conf["security.white"]=="false"
            print(f"PASS {prefix}: common native hook, physical defaults, original boot/identity/storage")
if __name__=="__main__":check(pathlib.Path(sys.argv[1]))
