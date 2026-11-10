/*
 * libdl_bio for ART without bionic_translation.
 *
 * ART loads JNI libraries with bionic_dlopen and friends, which
 * bionic_translation provides for loading bionic-linked libraries. Without
 * it, this fallback hands the calls to the system dynamic linker, which is
 * enough for libraries built for the host. A host that loads bionic
 * libraries itself (Mead) ships its own libdl_bio instead, ahead of this one
 * on the library path.
 */

#include <dlfcn.h>

#include "../../../bionic_translation/linker/dlfcn.h"

void dl_parse_library_path(const char *path, char *delim) {
    (void)path;
    (void)delim;
}

void *bionic_dlopen(const char *filename, int flag) {
    return dlopen(filename, flag);
}

const char *bionic_dlerror(void) {
    return dlerror();
}

void *bionic_dlsym(void *handle, const char *symbol) {
    return dlsym(handle, symbol);
}

int bionic_dlclose(void *handle) {
    return dlclose(handle);
}
