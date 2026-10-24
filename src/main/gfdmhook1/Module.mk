avsdlls += gfdmhook1

deplibs_gfdmhook1 := avs

ldflags_gfdmhook1 := -luser32 -ladvapi32

libs_gfdmhook1 := cconfig hook p3io security util

src_gfdmhook1 := \
    cabinet.c \
    config.c \
    dllmain.c \
    network.c \
    profile.c \
