/*
 * libmead_webkit: Mead's WebView engine (mead.webkit.MeadWebKit), WPE
 * WebKit rendering off screen.
 *
 * WPE runs on a thread of its own, with one web context for the process:
 * every JNI call is handed to that thread. A view renders through
 * WPEBackend-fdo's shared-memory export; each frame is copied (as RGBA,
 * premultiplied) for the Java view to draw, and the Java view is told.
 * Loads, errors, titles and script results go to the Java view from the
 * WPE thread, which stays attached to the VM.
 *
 * mead-asset: serves the app's assets (the Java view reads them).
 * JavaScript interfaces are the web-process extension's
 * (mead_webkit_extension.c); its calls come in on a Unix socket, each
 * connection on a thread of its own, and are made in Java there.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/un.h>
#include <wpe/webkit.h>
#include <wpe/fdo.h>
#include <wpe/unstable/fdo-shm.h>
#include <wpe/wpe.h>

#include "mead_webkit_bridge.h"

/* libwayland-server's shared-memory buffers (stable ABI). */
struct wl_shm_buffer;
void wl_shm_buffer_begin_access(struct wl_shm_buffer*);
void wl_shm_buffer_end_access(struct wl_shm_buffer*);
void* wl_shm_buffer_get_data(struct wl_shm_buffer*);
int32_t wl_shm_buffer_get_stride(struct wl_shm_buffer*);
int32_t wl_shm_buffer_get_width(struct wl_shm_buffer*);
int32_t wl_shm_buffer_get_height(struct wl_shm_buffer*);

static JavaVM* vm;
static jmethodID on_frame, on_load, on_load_failed, on_title, on_evaluated, invoke_method, read_asset;
static GMainContext* engine_context;
static WebKitWebContext* web_context;
static char socket_directory[96];
/* WPE's libraries bundled beside this library, when they are; NULL otherwise. */
static char* bundled_libraries;
static char socket_path[108];

typedef struct {
    jobject java;
    struct wpe_view_backend_exportable_fdo* exportable;
    struct wpe_view_backend* backend;
    WebKitWebView* view;
    guint64 page_id;
    pthread_mutex_t lock;            /* the frame and the interfaces */
    uint8_t* frame;
    int frame_width, frame_height;
    char** interfaces;
    int interface_count;
} View;

static pthread_mutex_t views_lock = PTHREAD_MUTEX_INITIALIZER;
static GPtrArray* views;

static JNIEnv* engine_env(void) {
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) == JNI_OK) return env;
    JavaVMAttachArgs args = {JNI_VERSION_1_6, "MeadWebKit", NULL};
    (*vm)->AttachCurrentThread(vm, &env, &args);
    return env;
}

static jstring string(JNIEnv* env, const char* text) {
    return text ? (*env)->NewStringUTF(env, text) : NULL;
}

/* Runs work on the WPE thread and waits for it. */
typedef struct {
    GSourceFunc work;
    gpointer data;
    gboolean done;
    GMutex mutex;
    GCond cond;
} Waiter;

static gboolean run_waited(gpointer data) {
    Waiter* waiter = data;
    waiter->work(waiter->data);
    g_mutex_lock(&waiter->mutex);
    waiter->done = TRUE;
    g_cond_signal(&waiter->cond);
    g_mutex_unlock(&waiter->mutex);
    return G_SOURCE_REMOVE;
}

static void on_engine(GSourceFunc work, gpointer data) {
    Waiter waiter = {.work = work, .data = data};
    g_mutex_init(&waiter.mutex);
    g_cond_init(&waiter.cond);
    g_main_context_invoke(engine_context, run_waited, &waiter);
    g_mutex_lock(&waiter.mutex);
    while (!waiter.done) g_cond_wait(&waiter.cond, &waiter.mutex);
    g_mutex_unlock(&waiter.mutex);
    g_mutex_clear(&waiter.mutex);
    g_cond_clear(&waiter.cond);
}

/* Frames. */

static void export_shm(void* data, struct wpe_fdo_shm_exported_buffer* exported) {
    View* view = data;
    struct wl_shm_buffer* buffer = wpe_fdo_shm_exported_buffer_get_shm_buffer(exported);
    int width = wl_shm_buffer_get_width(buffer), height = wl_shm_buffer_get_height(buffer);
    int stride = wl_shm_buffer_get_stride(buffer);
    wl_shm_buffer_begin_access(buffer);
    const uint8_t* pixels = wl_shm_buffer_get_data(buffer);
    pthread_mutex_lock(&view->lock);
    if (view->frame_width != width || view->frame_height != height) {
        free(view->frame);
        view->frame = malloc((size_t)width * height * 4);
        view->frame_width = width;
        view->frame_height = height;
    }
    /* ARGB8888, little-endian (B, G, R, A), to Android's ARGB_8888 (R, G, B, A). */
    for (int y = 0; y < height; y++) {
        const uint8_t* from = pixels + (size_t)y * stride;
        uint8_t* to = view->frame + (size_t)y * width * 4;
        for (int x = 0; x < width; x++, from += 4, to += 4) {
            to[0] = from[2];
            to[1] = from[1];
            to[2] = from[0];
            to[3] = from[3];
        }
    }
    pthread_mutex_unlock(&view->lock);
    wl_shm_buffer_end_access(buffer);
    wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(view->exportable, exported);
    wpe_view_backend_exportable_fdo_dispatch_frame_complete(view->exportable);
    JNIEnv* env = engine_env();
    (*env)->CallVoidMethod(env, view->java, on_frame);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static const struct wpe_view_backend_exportable_fdo_client exportable_client = {
    .export_shm_buffer = export_shm,
};

/* The page's events, to the Java view. */

static void load_changed(WebKitWebView* web_view, WebKitLoadEvent event, gpointer data) {
    View* view = data;
    int kind = event == WEBKIT_LOAD_STARTED ? 0 : event == WEBKIT_LOAD_COMMITTED ? 1
             : event == WEBKIT_LOAD_FINISHED ? 2 : -1;
    if (kind < 0) return;
    JNIEnv* env = engine_env();
    jstring uri = string(env, webkit_web_view_get_uri(web_view));
    (*env)->CallVoidMethod(env, view->java, on_load, kind, uri);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, uri);
}

static gboolean load_failed(WebKitWebView* web_view, WebKitLoadEvent event, const char* uri, GError* error,
                            gpointer data) {
    View* view = data;
    /* WebViewClient's codes: host lookup -2, connect -6, file not found -14, unknown -1. */
    int code = -1;
    if (error->domain == WEBKIT_NETWORK_ERROR) {
        code = error->code == WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST ? -14 : -6;
    } else if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND) {
        code = -14;
    }
    JNIEnv* env = engine_env();
    jstring juri = string(env, uri), message = string(env, error->message);
    (*env)->CallVoidMethod(env, view->java, on_load_failed, juri, code, message);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, juri);
    (*env)->DeleteLocalRef(env, message);
    return FALSE;
}

static void title_changed(GObject* object, GParamSpec* spec, gpointer data) {
    View* view = data;
    JNIEnv* env = engine_env();
    jstring title = string(env, webkit_web_view_get_title(WEBKIT_WEB_VIEW(object)));
    (*env)->CallVoidMethod(env, view->java, on_title, title);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, title);
}

/* The views, found by page or web view. */

static View* view_for_page(guint64 page_id) {
    View* found = NULL;
    pthread_mutex_lock(&views_lock);
    for (guint i = 0; views && i < views->len; i++) {
        View* view = g_ptr_array_index(views, i);
        if (view->page_id == page_id) found = view;
    }
    pthread_mutex_unlock(&views_lock);
    return found;
}

static View* view_for_web_view(WebKitWebView* web_view) {
    View* found = NULL;
    pthread_mutex_lock(&views_lock);
    for (guint i = 0; views && i < views->len; i++) {
        View* view = g_ptr_array_index(views, i);
        if (view->view == web_view) found = view;
    }
    pthread_mutex_unlock(&views_lock);
    return found;
}

/* mead-asset: the app's assets. */

static const char* mime_type(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    static const char* const types[][2] = {
        {".html", "text/html"}, {".htm", "text/html"}, {".js", "text/javascript"}, {".css", "text/css"},
        {".json", "application/json"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".svg", "image/svg+xml"}, {".webp", "image/webp"}, {".ttf", "font/ttf"},
        {".otf", "font/otf"}, {".woff", "font/woff"}, {".woff2", "font/woff2"}, {".txt", "text/plain"},
        {".xml", "application/xml"}, {".mp3", "audio/mpeg"}, {".ogg", "audio/ogg"}, {".wav", "audio/wav"},
        {".mp4", "video/mp4"},
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (g_ascii_strcasecmp(dot, types[i][0]) == 0) return types[i][1];
    }
    return "application/octet-stream";
}

static void asset_request(WebKitURISchemeRequest* request, gpointer data) {
    View* view = view_for_web_view(webkit_uri_scheme_request_get_web_view(request));
    const char* path = webkit_uri_scheme_request_get_path(request);
    while (path && *path == '/') path++;
    jbyteArray bytes = NULL;
    JNIEnv* env = engine_env();
    if (view && path) {
        jstring jpath = string(env, path);
        bytes = (*env)->CallObjectMethod(env, view->java, read_asset, jpath);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, jpath);
    }
    if (!bytes) {
        GError* error = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "No asset %s", path ? path : "");
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        return;
    }
    jsize length = (*env)->GetArrayLength(env, bytes);
    gpointer copy = g_malloc(length > 0 ? length : 1);
    (*env)->GetByteArrayRegion(env, bytes, 0, length, copy);
    (*env)->DeleteLocalRef(env, bytes);
    GInputStream* stream = g_memory_input_stream_new_from_data(copy, length, g_free);
    webkit_uri_scheme_request_finish(request, stream, length, mime_type(path));
    g_object_unref(stream);
}

/* The bridge: the extension's requests, answered from Java. */

static char** answer_request(JNIEnv* env, char** request, uint32_t count, uint32_t* answer_count) {
    *answer_count = 0;
    if (count >= 2 && strcmp(request[0], "interfaces") == 0) {
        View* view = view_for_page(g_ascii_strtoull(request[1], NULL, 10));
        if (!view) return calloc(1, sizeof(char*));
        pthread_mutex_lock(&view->lock);
        char** answer = calloc(view->interface_count + 1, sizeof(char*));
        for (int i = 0; i < view->interface_count; i++) answer[i] = strdup(view->interfaces[i]);
        *answer_count = view->interface_count;
        pthread_mutex_unlock(&view->lock);
        return answer;
    }
    if (count >= 4 && strcmp(request[0], "call") == 0) {
        View* view = view_for_page(g_ascii_strtoull(request[1], NULL, 10));
        char* result = NULL;
        if (view) {
            jclass string_class = (*env)->FindClass(env, "java/lang/String");
            jobjectArray arguments = (*env)->NewObjectArray(env, count - 4, string_class, NULL);
            for (uint32_t i = 4; i < count; i++) {
                jstring argument = string(env, request[i]);
                (*env)->SetObjectArrayElement(env, arguments, i - 4, argument);
                (*env)->DeleteLocalRef(env, argument);
            }
            jstring object = string(env, request[2]), method = string(env, request[3]);
            jstring json = (*env)->CallObjectMethod(env, view->java, invoke_method, object, method, arguments);
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
            if (json) {
                const char* chars = (*env)->GetStringUTFChars(env, json, NULL);
                result = strdup(chars);
                (*env)->ReleaseStringUTFChars(env, json, chars);
                (*env)->DeleteLocalRef(env, json);
            }
            (*env)->DeleteLocalRef(env, object);
            (*env)->DeleteLocalRef(env, method);
            (*env)->DeleteLocalRef(env, arguments);
            (*env)->DeleteLocalRef(env, string_class);
        }
        char** answer = calloc(2, sizeof(char*));
        answer[0] = result ? result : strdup("undefined");
        *answer_count = 1;
        return answer;
    }
    return calloc(1, sizeof(char*));
}

static void* serve_connection(void* data) {
    int fd = (int)(intptr_t)data;
    JNIEnv* env = NULL;
    JavaVMAttachArgs args = {JNI_VERSION_1_6, "JavaBridge", NULL};
    (*vm)->AttachCurrentThread(vm, &env, &args);
    for (;;) {
        uint32_t count = 0, answer_count = 0;
        char** request = mead_bridge_receive(fd, &count);
        if (!request) break;
        char** answer = answer_request(env, request, count, &answer_count);
        mead_bridge_free(request, count);
        int sent = mead_bridge_send(fd, (const char* const*)answer, answer_count);
        mead_bridge_free(answer, answer_count);
        if (sent != 0) break;
    }
    close(fd);
    (*vm)->DetachCurrentThread(vm);
    return NULL;
}

static void* serve(void* data) {
    int listener = (int)(intptr_t)data;
    for (;;) {
        int fd = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR) continue;
            return NULL;
        }
        pthread_t thread;
        pthread_create(&thread, NULL, serve_connection, (void*)(intptr_t)fd);
        pthread_detach(thread);
    }
}

/* The engine: its thread, and the web context every view shares. */

static GMutex start_mutex;
static GCond start_cond;
static gboolean started;

/*
 * WPE bundled beside this library (wpe/lib), on a host without it: its
 * helper processes find the bundle through their environment. (This
 * library finds it through its RPATH; WPE's helpers' own path is the
 * system's, which the launcher gives them on such a host.)
 */
static void use_bundled_wpe(void) {
    Dl_info info;
    if (!dladdr((void*)use_bundled_wpe, &info) || !info.dli_fname) return;
    char* directory = g_path_get_dirname(info.dli_fname);
    char* bundle = g_build_filename(directory, "wpe", "lib", NULL);
    if (g_file_test(bundle, G_FILE_TEST_IS_DIR)) {
        bundled_libraries = g_strdup(bundle);
        const char* existing = g_getenv("LD_LIBRARY_PATH");
        char* path = existing && *existing ? g_strconcat(bundle, ":", existing, NULL) : g_strdup(bundle);
        g_setenv("LD_LIBRARY_PATH", path, TRUE);
        g_free(path);
    }
    g_free(bundle);
    g_free(directory);
}

static void* engine_thread(void* data) {
    g_main_context_push_thread_default(engine_context);
    use_bundled_wpe();
    wpe_loader_init("libWPEBackend-fdo-1.0.so.1");
    wpe_fdo_initialize_shm();
    web_context = webkit_web_context_new();
    /* The extension sits beside this library, in webkit-extensions. */
    Dl_info info;
    if (dladdr((void*)engine_thread, &info) && info.dli_fname) {
        char* directory = g_path_get_dirname(info.dli_fname);
        char* extensions = g_build_filename(directory, "webkit-extensions", NULL);
        webkit_web_context_set_web_process_extensions_directory(web_context, extensions);
        g_free(extensions);
        g_free(directory);
    }
    webkit_web_context_set_web_process_extensions_initialization_user_data(web_context,
                                                                           g_variant_new_string(socket_path));
    /* WebKit sandboxes its web processes (bubblewrap) where it can: the
       bridge's socket must be in their view. */
    webkit_web_context_add_path_to_sandbox(web_context, socket_directory, FALSE);
    /* And the bundled libraries, read only, for the processes it starts. */
    if (bundled_libraries) webkit_web_context_add_path_to_sandbox(web_context, bundled_libraries, TRUE);
    webkit_web_context_register_uri_scheme(web_context, "mead-asset", asset_request, NULL, NULL);
    WebKitSecurityManager* security = webkit_web_context_get_security_manager(web_context);
    webkit_security_manager_register_uri_scheme_as_local(security, "mead-asset");
    webkit_security_manager_register_uri_scheme_as_secure(security, "mead-asset");
    webkit_security_manager_register_uri_scheme_as_cors_enabled(security, "mead-asset");
    g_mutex_lock(&start_mutex);
    started = TRUE;
    g_cond_signal(&start_cond);
    g_mutex_unlock(&start_mutex);
    GMainLoop* loop = g_main_loop_new(engine_context, FALSE);
    g_main_loop_run(loop);
    return NULL;
}

/*
 * Earlier processes' sockets, whose process has gone (however it ended): a
 * process can't count on removing its own as it exits.
 */
static void remove_stale_sockets(const char* runtime) {
    GDir* directory = g_dir_open(runtime, 0, NULL);
    if (!directory) return;
    const char* name;
    while ((name = g_dir_read_name(directory))) {
        if (!g_str_has_prefix(name, "mead-webkit-")) continue;
        pid_t pid = (pid_t)strtol(name + strlen("mead-webkit-"), NULL, 10);
        if (pid <= 0 || pid == getpid() || (kill(pid, 0) != 0 && errno == ESRCH)) {
            char* stale = g_build_filename(runtime, name, NULL);
            char* socket = g_build_filename(stale, "bridge", NULL);
            unlink(socket);
            rmdir(stale);
            g_free(socket);
            g_free(stale);
        }
    }
    g_dir_close(directory);
}

static void start_once(void) {
    {
        /* The runtime directory by its real path: WebKit's sandbox binds it by
           name, and a name through a symlink (ROCKNIX's /var/run, to /run)
           can't be laid out in the sandbox beside the link. Before GLib
           first reads it. */
        const char* runtime_name = getenv("XDG_RUNTIME_DIR");
        char* real_runtime = runtime_name ? realpath(runtime_name, NULL) : NULL;
        if (real_runtime) setenv("XDG_RUNTIME_DIR", real_runtime, 1);
        free(real_runtime);
        /* In a directory of its own, which the web processes' sandbox is given. */
        const char* runtime = g_get_user_runtime_dir() ? g_get_user_runtime_dir() : "/tmp";
        remove_stale_sockets(runtime);
        snprintf(socket_directory, sizeof(socket_directory), "%s/mead-webkit-%d", runtime, getpid());
        g_mkdir_with_parents(socket_directory, 0700);
        snprintf(socket_path, sizeof(socket_path), "%s/bridge", socket_directory);
        unlink(socket_path);
        int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        struct sockaddr_un address = {.sun_family = AF_UNIX};
        snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);
        if (bind(listener, (struct sockaddr*)&address, sizeof(address)) == 0 && listen(listener, 8) == 0) {
            pthread_t thread;
            pthread_create(&thread, NULL, serve, (void*)(intptr_t)listener);
            pthread_detach(thread);
        }
        views = g_ptr_array_new();
        engine_context = g_main_context_new();
        pthread_t thread;
        pthread_create(&thread, NULL, engine_thread, NULL);
        pthread_detach(thread);
        g_mutex_lock(&start_mutex);
        while (!started) g_cond_wait(&start_cond, &start_mutex);
        g_mutex_unlock(&start_mutex);
    }
}

static void start_engine(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, start_once);
}

/* JNI. */

typedef struct {
    View* view;
    int width, height;
    const char* text;
    const char* text2;
    gboolean result;
    int number;
    double x, y;
    gboolean flag, flag2;
    int action, button, keysym, unicode, modifiers;
} Call;

static gboolean create_on_engine(gpointer data) {
    Call* call = data;
    View* view = call->view;
    view->exportable = wpe_view_backend_exportable_fdo_create(&exportable_client, view, call->width, call->height);
    view->backend = wpe_view_backend_exportable_fdo_get_view_backend(view->exportable);
    WebKitWebViewBackend* backend = webkit_web_view_backend_new(view->backend, NULL, NULL);
    view->view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "backend", backend, "web-context", web_context,
                                              NULL));
    view->page_id = webkit_web_view_get_page_id(view->view);
    wpe_view_backend_add_activity_state(view->backend, wpe_view_activity_state_visible |
                                                       wpe_view_activity_state_in_window |
                                                       wpe_view_activity_state_focused);
    g_signal_connect(view->view, "load-changed", G_CALLBACK(load_changed), view);
    g_signal_connect(view->view, "load-failed", G_CALLBACK(load_failed), view);
    g_signal_connect(view->view, "notify::title", G_CALLBACK(title_changed), view);
    return G_SOURCE_REMOVE;
}

static jlong native_create(JNIEnv* env, jclass clazz, jobject java, jint width, jint height) {
    start_engine();
    View* view = calloc(1, sizeof(View));
    view->java = (*env)->NewGlobalRef(env, java);
    pthread_mutex_init(&view->lock, NULL);
    Call call = {.view = view, .width = width > 0 ? width : 1, .height = height > 0 ? height : 1};
    on_engine(create_on_engine, &call);
    pthread_mutex_lock(&views_lock);
    g_ptr_array_add(views, view);
    pthread_mutex_unlock(&views_lock);
    return (jlong)(intptr_t)view;
}

static gboolean destroy_on_engine(gpointer data) {
    View* view = ((Call*)data)->view;
    g_object_unref(view->view);
    wpe_view_backend_exportable_fdo_destroy(view->exportable);
    return G_SOURCE_REMOVE;
}

static void native_destroy(JNIEnv* env, jclass clazz, jlong handle) {
    View* view = (View*)(intptr_t)handle;
    pthread_mutex_lock(&views_lock);
    g_ptr_array_remove(views, view);
    pthread_mutex_unlock(&views_lock);
    Call call = {.view = view};
    on_engine(destroy_on_engine, &call);
    (*env)->DeleteGlobalRef(env, view->java);
    for (int i = 0; i < view->interface_count; i++) free(view->interfaces[i]);
    free(view->interfaces);
    free(view->frame);
    free(view);
}

#define TEXT_CALL(name, body)                                                         \
    static gboolean name##_on_engine(gpointer data) {                                \
        Call* call = data;                                                           \
        body;                                                                        \
        return G_SOURCE_REMOVE;                                                      \
    }

TEXT_CALL(load_url, webkit_web_view_load_uri(call->view->view, call->text))
TEXT_CALL(load_html, webkit_web_view_load_html(call->view->view, call->text, call->text2))
TEXT_CALL(stop, webkit_web_view_stop_loading(call->view->view))
TEXT_CALL(reload, webkit_web_view_reload(call->view->view))
TEXT_CALL(go_back, webkit_web_view_go_back(call->view->view))
TEXT_CALL(go_forward, webkit_web_view_go_forward(call->view->view))
TEXT_CALL(can_go_back, call->result = webkit_web_view_can_go_back(call->view->view))
TEXT_CALL(can_go_forward, call->result = webkit_web_view_can_go_forward(call->view->view))
TEXT_CALL(resize, wpe_view_backend_dispatch_set_size(call->view->backend, call->width, call->height))

static void with_text(JNIEnv* env, jlong handle, jstring text, jstring text2, GSourceFunc work) {
    Call call = {.view = (View*)(intptr_t)handle};
    call.text = text ? (*env)->GetStringUTFChars(env, text, NULL) : NULL;
    call.text2 = text2 ? (*env)->GetStringUTFChars(env, text2, NULL) : NULL;
    on_engine(work, &call);
    if (text) (*env)->ReleaseStringUTFChars(env, text, call.text);
    if (text2) (*env)->ReleaseStringUTFChars(env, text2, call.text2);
}

static void native_load_url(JNIEnv* env, jclass c, jlong h, jstring url) { with_text(env, h, url, NULL, load_url_on_engine); }
static void native_load_html(JNIEnv* env, jclass c, jlong h, jstring html, jstring base) {
    with_text(env, h, html, base, load_html_on_engine);
}
static void native_stop(JNIEnv* env, jclass c, jlong h) { with_text(env, h, NULL, NULL, stop_on_engine); }
static void native_reload(JNIEnv* env, jclass c, jlong h) { with_text(env, h, NULL, NULL, reload_on_engine); }
static void native_go_back(JNIEnv* env, jclass c, jlong h) { with_text(env, h, NULL, NULL, go_back_on_engine); }
static void native_go_forward(JNIEnv* env, jclass c, jlong h) { with_text(env, h, NULL, NULL, go_forward_on_engine); }

static jboolean native_can_go_back(JNIEnv* env, jclass c, jlong h) {
    Call call = {.view = (View*)(intptr_t)h};
    on_engine(can_go_back_on_engine, &call);
    return call.result;
}

static jboolean native_can_go_forward(JNIEnv* env, jclass c, jlong h) {
    Call call = {.view = (View*)(intptr_t)h};
    on_engine(can_go_forward_on_engine, &call);
    return call.result;
}

static void native_resize(JNIEnv* env, jclass c, jlong h, jint width, jint height) {
    Call call = {.view = (View*)(intptr_t)h, .width = width, .height = height};
    on_engine(resize_on_engine, &call);
}

static gboolean background_on_engine(gpointer data) {
    Call* call = data;
    uint32_t argb = (uint32_t)call->number;
    WebKitColor color = {((argb >> 16) & 0xff) / 255.0, ((argb >> 8) & 0xff) / 255.0, (argb & 0xff) / 255.0,
                         ((argb >> 24) & 0xff) / 255.0};
    webkit_web_view_set_background_color(call->view->view, &color);
    return G_SOURCE_REMOVE;
}

static void native_set_background(JNIEnv* env, jclass c, jlong h, jint argb) {
    Call call = {.view = (View*)(intptr_t)h, .number = argb};
    on_engine(background_on_engine, &call);
}

static gboolean settings_on_engine(gpointer data) {
    Call* call = data;
    WebKitSettings* settings = webkit_web_view_get_settings(call->view->view);
    webkit_settings_set_enable_javascript(settings, call->flag);
    webkit_settings_set_allow_file_access_from_file_urls(settings, call->flag2);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, call->result);
    if (call->text) webkit_settings_set_user_agent(settings, call->text);
    return G_SOURCE_REMOVE;
}

static void native_set_settings(JNIEnv* env, jclass c, jlong h, jboolean javascript, jboolean file_access,
                                jboolean universal_access, jstring user_agent) {
    Call call = {.view = (View*)(intptr_t)h, .flag = javascript, .flag2 = file_access, .result = universal_access};
    call.text = user_agent ? (*env)->GetStringUTFChars(env, user_agent, NULL) : NULL;
    on_engine(settings_on_engine, &call);
    if (user_agent) (*env)->ReleaseStringUTFChars(env, user_agent, call.text);
}

typedef struct {
    View* view;
    int id;
} Evaluation;

static void evaluated(GObject* object, GAsyncResult* result, gpointer data) {
    Evaluation* evaluation = data;
    GError* error = NULL;
    JSCValue* value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);
    char* json = value && !jsc_value_is_undefined(value) ? jsc_value_to_json(value, 0) : NULL;
    JNIEnv* env = engine_env();
    jstring jjson = string(env, json ? json : "null");
    (*env)->CallVoidMethod(env, evaluation->view->java, on_evaluated, evaluation->id, jjson);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, jjson);
    g_free(json);
    if (value) g_object_unref(value);
    if (error) g_error_free(error);
    g_free(evaluation);
}

static gboolean evaluate_on_engine(gpointer data) {
    Call* call = data;
    Evaluation* evaluation = g_new0(Evaluation, 1);
    evaluation->view = call->view;
    evaluation->id = call->number;
    webkit_web_view_evaluate_javascript(call->view->view, call->text, -1, NULL, NULL, NULL, evaluated, evaluation);
    return G_SOURCE_REMOVE;
}

static void native_evaluate(JNIEnv* env, jclass c, jlong h, jstring script, jint id) {
    Call call = {.view = (View*)(intptr_t)h, .number = id};
    call.text = (*env)->GetStringUTFChars(env, script, NULL);
    on_engine(evaluate_on_engine, &call);
    (*env)->ReleaseStringUTFChars(env, script, call.text);
}

static void native_set_interfaces(JNIEnv* env, jclass c, jlong h, jobjectArray interfaces) {
    View* view = (View*)(intptr_t)h;
    int count = interfaces ? (*env)->GetArrayLength(env, interfaces) : 0;
    char** described = calloc(count + 1, sizeof(char*));
    for (int i = 0; i < count; i++) {
        jstring each = (*env)->GetObjectArrayElement(env, interfaces, i);
        const char* chars = (*env)->GetStringUTFChars(env, each, NULL);
        described[i] = strdup(chars);
        (*env)->ReleaseStringUTFChars(env, each, chars);
        (*env)->DeleteLocalRef(env, each);
    }
    pthread_mutex_lock(&view->lock);
    for (int i = 0; i < view->interface_count; i++) free(view->interfaces[i]);
    free(view->interfaces);
    view->interfaces = described;
    view->interface_count = count;
    pthread_mutex_unlock(&view->lock);
}

static uint32_t now_ms(void) {
    return (uint32_t)(g_get_monotonic_time() / 1000);
}

static gboolean pointer_on_engine(gpointer data) {
    Call* call = data;
    struct wpe_input_pointer_event event = {
        .type = call->action == 2 ? wpe_input_pointer_event_type_motion : wpe_input_pointer_event_type_button,
        .time = now_ms(),
        .x = (int)call->x,
        .y = (int)call->y,
        .button = call->action == 2 ? 0 : (uint32_t)call->button,
        .state = call->action == 0 ? 1 : 0,
        .modifiers = 0,
    };
    wpe_view_backend_dispatch_pointer_event(call->view->backend, &event);
    return G_SOURCE_REMOVE;
}

static void native_pointer(JNIEnv* env, jclass c, jlong h, jint action, jint x, jint y, jint button) {
    Call call = {.view = (View*)(intptr_t)h, .action = action, .x = x, .y = y, .button = button};
    on_engine(pointer_on_engine, &call);
}

static gboolean scroll_on_engine(gpointer data) {
    Call* call = data;
    /* Android's scroll steps are a line each; a wheel's step in WPE is a line too. */
    if (call->flag) {
        struct wpe_input_axis_event event = {wpe_input_axis_event_type_motion, now_ms(), call->number, call->button,
                                             0, (int32_t)(-call->y * 1), 0};
        wpe_view_backend_dispatch_axis_event(call->view->backend, &event);
    }
    if (call->flag2) {
        struct wpe_input_axis_event event = {wpe_input_axis_event_type_motion, now_ms(), call->number, call->button,
                                             1, (int32_t)(call->x * 1), 0};
        wpe_view_backend_dispatch_axis_event(call->view->backend, &event);
    }
    return G_SOURCE_REMOVE;
}

static void native_scroll(JNIEnv* env, jclass c, jlong h, jint x, jint y, jfloat dx, jfloat dy) {
    Call call = {.view = (View*)(intptr_t)h, .number = x, .button = y, .x = dx, .y = dy,
                 .flag = dy != 0, .flag2 = dx != 0};
    on_engine(scroll_on_engine, &call);
}

static gboolean key_on_engine(gpointer data) {
    Call* call = data;
    /* Android's meta state: shift 1, alt 2, control 0x1000, meta 0x10000. */
    uint32_t modifiers = 0;
    if (call->modifiers & 0x1) modifiers |= wpe_input_keyboard_modifier_shift;
    if (call->modifiers & 0x2) modifiers |= wpe_input_keyboard_modifier_alt;
    if (call->modifiers & 0x1000) modifiers |= wpe_input_keyboard_modifier_control;
    if (call->modifiers & 0x10000) modifiers |= wpe_input_keyboard_modifier_meta;
    struct wpe_input_keyboard_event event = {now_ms(), (uint32_t)call->keysym, 0, call->flag, modifiers};
    wpe_view_backend_dispatch_keyboard_event(call->view->backend, &event);
    return G_SOURCE_REMOVE;
}

static void native_key(JNIEnv* env, jclass c, jlong h, jint keysym, jint unicode, jboolean down, jint modifiers) {
    Call call = {.view = (View*)(intptr_t)h, .keysym = keysym, .unicode = unicode, .flag = down,
                 .modifiers = modifiers};
    on_engine(key_on_engine, &call);
}

static jboolean native_copy_frame(JNIEnv* env, jclass c, jlong h, jobject buffer, jint width, jint height) {
    View* view = (View*)(intptr_t)h;
    uint8_t* to = (*env)->GetDirectBufferAddress(env, buffer);
    if (!to || width <= 0 || height <= 0) return JNI_FALSE;
    pthread_mutex_lock(&view->lock);
    jboolean copied = JNI_FALSE;
    if (view->frame) {
        int rows = view->frame_height < height ? view->frame_height : height;
        int columns = view->frame_width < width ? view->frame_width : width;
        if (rows < height || columns < width) memset(to, 0, (size_t)width * height * 4);
        for (int y = 0; y < rows; y++) {
            memcpy(to + (size_t)y * width * 4, view->frame + (size_t)y * view->frame_width * 4, (size_t)columns * 4);
        }
        copied = JNI_TRUE;
    }
    pthread_mutex_unlock(&view->lock);
    return copied;
}

static const JNINativeMethod methods[] = {
    {"nativeCreate", "(Lmead/webkit/MeadWebView;II)J", (void*)native_create},
    {"nativeDestroy", "(J)V", (void*)native_destroy},
    {"nativeLoadUrl", "(JLjava/lang/String;)V", (void*)native_load_url},
    {"nativeLoadHtml", "(JLjava/lang/String;Ljava/lang/String;)V", (void*)native_load_html},
    {"nativeStop", "(J)V", (void*)native_stop},
    {"nativeReload", "(J)V", (void*)native_reload},
    {"nativeGoBack", "(J)V", (void*)native_go_back},
    {"nativeGoForward", "(J)V", (void*)native_go_forward},
    {"nativeCanGoBack", "(J)Z", (void*)native_can_go_back},
    {"nativeCanGoForward", "(J)Z", (void*)native_can_go_forward},
    {"nativeResize", "(JII)V", (void*)native_resize},
    {"nativeSetBackground", "(JI)V", (void*)native_set_background},
    {"nativeSetSettings", "(JZZZLjava/lang/String;)V", (void*)native_set_settings},
    {"nativeEvaluate", "(JLjava/lang/String;I)V", (void*)native_evaluate},
    {"nativeSetInterfaces", "(J[Ljava/lang/String;)V", (void*)native_set_interfaces},
    {"nativePointer", "(JIIII)V", (void*)native_pointer},
    {"nativeScroll", "(JIIFF)V", (void*)native_scroll},
    {"nativeKey", "(JIIZI)V", (void*)native_key},
    {"nativeCopyFrame", "(JLjava/nio/ByteBuffer;II)Z", (void*)native_copy_frame},
};

JNIEXPORT jint JNI_OnLoad(JavaVM* java_vm, void* reserved) {
    vm = java_vm;
    JNIEnv* env;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass kit = (*env)->FindClass(env, "mead/webkit/MeadWebKit");
    jclass view = (*env)->FindClass(env, "mead/webkit/MeadWebView");
    if (!kit || !view) return JNI_ERR;
    if ((*env)->RegisterNatives(env, kit, methods, sizeof(methods) / sizeof(methods[0])) != 0) return JNI_ERR;
    on_frame = (*env)->GetMethodID(env, view, "onFrame", "()V");
    on_load = (*env)->GetMethodID(env, view, "onLoad", "(ILjava/lang/String;)V");
    on_load_failed = (*env)->GetMethodID(env, view, "onLoadFailed", "(Ljava/lang/String;ILjava/lang/String;)V");
    on_title = (*env)->GetMethodID(env, view, "onTitle", "(Ljava/lang/String;)V");
    on_evaluated = (*env)->GetMethodID(env, view, "onEvaluated", "(ILjava/lang/String;)V");
    invoke_method = (*env)->GetMethodID(env, view, "invoke",
                                        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
    read_asset = (*env)->GetMethodID(env, view, "readAsset", "(Ljava/lang/String;)[B");
    if (!on_frame || !on_load || !on_load_failed || !on_title || !on_evaluated || !invoke_method || !read_asset) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
