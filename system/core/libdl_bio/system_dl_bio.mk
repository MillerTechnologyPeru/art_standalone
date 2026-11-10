# The system libdl_bio (from bionic_translation), if one is installed.
ART_SYSTEM_DL_BIO := $(firstword $(wildcard \
        /usr/lib/libdl_bio.so /usr/lib/*/libdl_bio.so \
        /usr/local/lib/libdl_bio.so /usr/local/lib/*/libdl_bio.so))
