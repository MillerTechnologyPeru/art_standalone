/*
 * libcutils' qtaguid: Android's per-socket traffic accounting, through its
 * kernels' xt_qtaguid module, which other kernels don't have. Sockets are
 * tagged and untagged as asked, and nothing is counted.
 */

#include <sys/types.h>

#include <cutils/qtaguid.h>

extern "C" {

int qtaguid_tagSocket(int, int, uid_t) {
    return 0;
}

int qtaguid_untagSocket(int) {
    return 0;
}

int qtaguid_setCounterSet(int, uid_t) {
    return 0;
}

int qtaguid_deleteTagData(int, uid_t) {
    return 0;
}

int qtaguid_setPacifier(int) {
    return 0;
}

}
