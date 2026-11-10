/*
 * bionic's <sys/system_properties.h>, as far as Android 10's framework code
 * uses it off Android. libbase implements these functions in-process when
 * it isn't built for bionic; Mead's bionic provider, when it is loaded,
 * answers them for the whole process.
 */

#ifndef MEAD_SYS_SYSTEM_PROPERTIES_H
#define MEAD_SYS_SYSTEM_PROPERTIES_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROP_NAME_MAX 32
#define PROP_VALUE_MAX 92

int __system_property_get(const char* name, char* value);
int __system_property_set(const char* name, const char* value);

#ifdef __cplusplus
}
#endif

#endif
