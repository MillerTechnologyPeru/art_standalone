/*
 * Mead's WPE web-process extension: an Android app's JavaScript interfaces
 * (WebView.addJavascriptInterface) in its pages. When a frame's window
 * object is made, it asks Mead which interfaces the page has, and defines
 * each as an object whose methods call Mead and wait for the result, as
 * Android's calls do (mead_webkit_bridge.h). Its initialization data is the
 * socket to Mead.
 */

#include <wpe/webkit-web-process-extension.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "mead_webkit_bridge.h"

static char* socket_path;
static int connection = -1;
static pthread_mutex_t connection_lock = PTHREAD_MUTEX_INITIALIZER;

/* Sends a request and waits for its answer; NULL when Mead can't be reached. */
static char** ask(const char* const* request, uint32_t count, uint32_t* answer_count) {
    pthread_mutex_lock(&connection_lock);
    if (connection < 0) {
        connection = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        struct sockaddr_un address = {.sun_family = AF_UNIX};
        snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);
        if (connect(connection, (struct sockaddr*)&address, sizeof(address)) != 0) {
            close(connection);
            connection = -1;
        }
    }
    char** answer = NULL;
    if (connection >= 0 && mead_bridge_send(connection, request, count) == 0) {
        answer = mead_bridge_receive(connection, answer_count);
    }
    if (!answer && connection >= 0) {
        close(connection);
        connection = -1;
    }
    pthread_mutex_unlock(&connection_lock);
    return answer;
}

typedef struct {
    char* page;
    char* object;
    char* method;
} Method;

static void method_free(gpointer data) {
    Method* method = data;
    g_free(method->page);
    g_free(method->object);
    g_free(method->method);
    g_free(method);
}

/* A call to an interface's method: its arguments as JSON to Mead, its result back. */
static JSCValue* call(GPtrArray* arguments, gpointer data) {
    Method* method = data;
    JSCContext* context = jsc_context_get_current();
    uint32_t count = 4 + arguments->len;
    char** request = g_new0(char*, count);
    request[0] = g_strdup("call");
    request[1] = g_strdup(method->page);
    request[2] = g_strdup(method->object);
    request[3] = g_strdup(method->method);
    for (guint i = 0; i < arguments->len; i++) {
        JSCValue* argument = g_ptr_array_index(arguments, i);
        char* json = jsc_value_is_undefined(argument) ? NULL : jsc_value_to_json(argument, 0);
        request[4 + i] = json ? json : g_strdup("undefined");
    }
    uint32_t answer_count = 0;
    char** answer = ask((const char* const*)request, count, &answer_count);
    for (uint32_t i = 0; i < count; i++) g_free(request[i]);
    g_free(request);
    JSCValue* result;
    if (answer && answer_count == 1 && strcmp(answer[0], "undefined") != 0) {
        result = jsc_value_new_from_json(context, answer[0]);
    } else {
        result = jsc_value_new_undefined(context);
    }
    mead_bridge_free(answer, answer_count);
    return result;
}

static void window_object_cleared(WebKitScriptWorld* world, WebKitWebPage* page, WebKitFrame* frame, gpointer data) {
    char page_id[32];
    snprintf(page_id, sizeof(page_id), "%" G_GUINT64_FORMAT, webkit_web_page_get_id(page));
    const char* request[] = {"interfaces", page_id};
    uint32_t count = 0;
    char** interfaces = ask(request, 2, &count);
    if (!interfaces) return;
    JSCContext* context = webkit_frame_get_js_context_for_script_world(frame, world);
    for (uint32_t i = 0; i < count; i++) {
        char* colon = strchr(interfaces[i], ':');
        if (!colon) continue;
        *colon = 0;
        const char* name = interfaces[i];
        JSCValue* object = jsc_value_new_object(context, NULL, NULL);
        char** methods = g_strsplit(colon + 1, ",", -1);
        for (char** each = methods; *each; each++) {
            if (!**each) continue;
            Method* method = g_new0(Method, 1);
            method->page = g_strdup(page_id);
            method->object = g_strdup(name);
            method->method = g_strdup(*each);
            JSCValue* function = jsc_value_new_function_variadic(context, *each, G_CALLBACK(call), method,
                                                                 method_free, JSC_TYPE_VALUE);
            jsc_value_object_set_property(object, *each, function);
            g_object_unref(function);
        }
        g_strfreev(methods);
        jsc_context_set_value(context, name, object);
        g_object_unref(object);
    }
    g_object_unref(context);
    mead_bridge_free(interfaces, count);
}

G_MODULE_EXPORT void webkit_web_process_extension_initialize_with_user_data(WebKitWebProcessExtension* extension,
                                                                            const GVariant* data) {
    socket_path = g_variant_dup_string((GVariant*)data, NULL);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared",
                     G_CALLBACK(window_object_cleared), NULL);
}
