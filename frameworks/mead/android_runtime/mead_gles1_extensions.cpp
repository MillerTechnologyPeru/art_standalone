/*
 * The OpenGL ES 1 extension functions GLImpl (GL10) calls, for hosts whose
 * libGLESv1_CM exports only the core ones: each is the driver's, from
 * eglGetProcAddress, the first time it's called. One a driver doesn't have
 * does nothing and returns 0, as a call to an extension that isn't there
 * would on Android.
 */

#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

namespace {

template <typename F>
F resolve(F* slot, const char* name) {
    if (*slot == nullptr) *slot = reinterpret_cast<F>(eglGetProcAddress(name));
    return *slot;
}

}  // namespace

extern "C" {

GL_API void GL_APIENTRY glBindFramebufferOES(GLenum target, GLuint framebuffer) {
    static void (GL_APIENTRY *f)(GLenum target, GLuint framebuffer) = nullptr;
    if (auto call = resolve(&f, "glBindFramebufferOES")) call(target, framebuffer);
}

GL_API void GL_APIENTRY glBindRenderbufferOES(GLenum target, GLuint renderbuffer) {
    static void (GL_APIENTRY *f)(GLenum target, GLuint renderbuffer) = nullptr;
    if (auto call = resolve(&f, "glBindRenderbufferOES")) call(target, renderbuffer);
}

GL_API GLenum GL_APIENTRY glCheckFramebufferStatusOES(GLenum target) {
    static GLenum (GL_APIENTRY *f)(GLenum target) = nullptr;
    auto call = resolve(&f, "glCheckFramebufferStatusOES");
    return call ? call(target) : 0;
}

GL_API void GL_APIENTRY glCurrentPaletteMatrixOES(GLuint matrixpaletteindex) {
    static void (GL_APIENTRY *f)(GLuint matrixpaletteindex) = nullptr;
    if (auto call = resolve(&f, "glCurrentPaletteMatrixOES")) call(matrixpaletteindex);
}

GL_API void GL_APIENTRY glDeleteFramebuffersOES(GLsizei n, const GLuint *framebuffers) {
    static void (GL_APIENTRY *f)(GLsizei n, const GLuint *framebuffers) = nullptr;
    if (auto call = resolve(&f, "glDeleteFramebuffersOES")) call(n, framebuffers);
}

GL_API void GL_APIENTRY glDeleteRenderbuffersOES(GLsizei n, const GLuint *renderbuffers) {
    static void (GL_APIENTRY *f)(GLsizei n, const GLuint *renderbuffers) = nullptr;
    if (auto call = resolve(&f, "glDeleteRenderbuffersOES")) call(n, renderbuffers);
}

GL_API void GL_APIENTRY glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height) {
    static void (GL_APIENTRY *f)(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height) = nullptr;
    if (auto call = resolve(&f, "glDrawTexfOES")) call(x, y, z, width, height);
}

GL_API void GL_APIENTRY glDrawTexfvOES(const GLfloat *coords) {
    static void (GL_APIENTRY *f)(const GLfloat *coords) = nullptr;
    if (auto call = resolve(&f, "glDrawTexfvOES")) call(coords);
}

GL_API void GL_APIENTRY glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height) {
    static void (GL_APIENTRY *f)(GLint x, GLint y, GLint z, GLint width, GLint height) = nullptr;
    if (auto call = resolve(&f, "glDrawTexiOES")) call(x, y, z, width, height);
}

GL_API void GL_APIENTRY glDrawTexivOES(const GLint *coords) {
    static void (GL_APIENTRY *f)(const GLint *coords) = nullptr;
    if (auto call = resolve(&f, "glDrawTexivOES")) call(coords);
}

GL_API void GL_APIENTRY glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height) {
    static void (GL_APIENTRY *f)(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height) = nullptr;
    if (auto call = resolve(&f, "glDrawTexsOES")) call(x, y, z, width, height);
}

GL_API void GL_APIENTRY glDrawTexsvOES(const GLshort *coords) {
    static void (GL_APIENTRY *f)(const GLshort *coords) = nullptr;
    if (auto call = resolve(&f, "glDrawTexsvOES")) call(coords);
}

GL_API void GL_APIENTRY glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height) {
    static void (GL_APIENTRY *f)(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height) = nullptr;
    if (auto call = resolve(&f, "glDrawTexxOES")) call(x, y, z, width, height);
}

GL_API void GL_APIENTRY glDrawTexxvOES(const GLfixed *coords) {
    static void (GL_APIENTRY *f)(const GLfixed *coords) = nullptr;
    if (auto call = resolve(&f, "glDrawTexxvOES")) call(coords);
}

GL_API void GL_APIENTRY glFramebufferRenderbufferOES(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    static void (GL_APIENTRY *f)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) = nullptr;
    if (auto call = resolve(&f, "glFramebufferRenderbufferOES")) call(target, attachment, renderbuffertarget, renderbuffer);
}

GL_API void GL_APIENTRY glFramebufferTexture2DOES(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    static void (GL_APIENTRY *f)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) = nullptr;
    if (auto call = resolve(&f, "glFramebufferTexture2DOES")) call(target, attachment, textarget, texture, level);
}

GL_API void GL_APIENTRY glGenFramebuffersOES(GLsizei n, GLuint *framebuffers) {
    static void (GL_APIENTRY *f)(GLsizei n, GLuint *framebuffers) = nullptr;
    if (auto call = resolve(&f, "glGenFramebuffersOES")) call(n, framebuffers);
}

GL_API void GL_APIENTRY glGenRenderbuffersOES(GLsizei n, GLuint *renderbuffers) {
    static void (GL_APIENTRY *f)(GLsizei n, GLuint *renderbuffers) = nullptr;
    if (auto call = resolve(&f, "glGenRenderbuffersOES")) call(n, renderbuffers);
}

GL_API void GL_APIENTRY glGenerateMipmapOES(GLenum target) {
    static void (GL_APIENTRY *f)(GLenum target) = nullptr;
    if (auto call = resolve(&f, "glGenerateMipmapOES")) call(target);
}

GL_API void GL_APIENTRY glGetFramebufferAttachmentParameterivOES(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    static void (GL_APIENTRY *f)(GLenum target, GLenum attachment, GLenum pname, GLint *params) = nullptr;
    if (auto call = resolve(&f, "glGetFramebufferAttachmentParameterivOES")) call(target, attachment, pname, params);
}

GL_API void GL_APIENTRY glGetRenderbufferParameterivOES(GLenum target, GLenum pname, GLint *params) {
    static void (GL_APIENTRY *f)(GLenum target, GLenum pname, GLint *params) = nullptr;
    if (auto call = resolve(&f, "glGetRenderbufferParameterivOES")) call(target, pname, params);
}

GL_API GLboolean GL_APIENTRY glIsFramebufferOES(GLuint framebuffer) {
    static GLboolean (GL_APIENTRY *f)(GLuint framebuffer) = nullptr;
    auto call = resolve(&f, "glIsFramebufferOES");
    return call ? call(framebuffer) : 0;
}

GL_API GLboolean GL_APIENTRY glIsRenderbufferOES(GLuint renderbuffer) {
    static GLboolean (GL_APIENTRY *f)(GLuint renderbuffer) = nullptr;
    auto call = resolve(&f, "glIsRenderbufferOES");
    return call ? call(renderbuffer) : 0;
}

GL_API void GL_APIENTRY glLoadPaletteFromModelViewMatrixOES(void) {
    static void (GL_APIENTRY *f)(void) = nullptr;
    if (auto call = resolve(&f, "glLoadPaletteFromModelViewMatrixOES")) call();
}

GL_API void GL_APIENTRY glMatrixIndexPointerOES(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    static void (GL_APIENTRY *f)(GLint size, GLenum type, GLsizei stride, const void *pointer) = nullptr;
    if (auto call = resolve(&f, "glMatrixIndexPointerOES")) call(size, type, stride, pointer);
}

GL_API GLbitfield GL_APIENTRY glQueryMatrixxOES(GLfixed *mantissa, GLint *exponent) {
    static GLbitfield (GL_APIENTRY *f)(GLfixed *mantissa, GLint *exponent) = nullptr;
    auto call = resolve(&f, "glQueryMatrixxOES");
    return call ? call(mantissa, exponent) : 0;
}

GL_API void GL_APIENTRY glRenderbufferStorageOES(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    static void (GL_APIENTRY *f)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) = nullptr;
    if (auto call = resolve(&f, "glRenderbufferStorageOES")) call(target, internalformat, width, height);
}

GL_API void GL_APIENTRY glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    static void (GL_APIENTRY *f)(GLint size, GLenum type, GLsizei stride, const void *pointer) = nullptr;
    if (auto call = resolve(&f, "glWeightPointerOES")) call(size, type, stride, pointer);
}

GL_API void GL_APIENTRY glBlendEquationOES(GLenum mode) {
    static void (GL_APIENTRY *f)(GLenum mode) = nullptr;
    if (auto call = resolve(&f, "glBlendEquationOES")) call(mode);
}

GL_API void GL_APIENTRY glBlendEquationSeparateOES(GLenum modeRGB, GLenum modeAlpha) {
    static void (GL_APIENTRY *f)(GLenum modeRGB, GLenum modeAlpha) = nullptr;
    if (auto call = resolve(&f, "glBlendEquationSeparateOES")) call(modeRGB, modeAlpha);
}

GL_API void GL_APIENTRY glBlendFuncSeparateOES(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    static void (GL_APIENTRY *f)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = nullptr;
    if (auto call = resolve(&f, "glBlendFuncSeparateOES")) call(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

GL_API void GL_APIENTRY glGetTexGenfvOES(GLenum coord, GLenum pname, GLfloat *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLfloat *params) = nullptr;
    if (auto call = resolve(&f, "glGetTexGenfvOES")) call(coord, pname, params);
}

GL_API void GL_APIENTRY glGetTexGenivOES(GLenum coord, GLenum pname, GLint *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLint *params) = nullptr;
    if (auto call = resolve(&f, "glGetTexGenivOES")) call(coord, pname, params);
}

GL_API void GL_APIENTRY glGetTexGenxvOES(GLenum coord, GLenum pname, GLfixed *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLfixed *params) = nullptr;
    if (auto call = resolve(&f, "glGetTexGenxvOES")) call(coord, pname, params);
}

GL_API void GL_APIENTRY glTexGenfOES(GLenum coord, GLenum pname, GLfloat param) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLfloat param) = nullptr;
    if (auto call = resolve(&f, "glTexGenfOES")) call(coord, pname, param);
}

GL_API void GL_APIENTRY glTexGenfvOES(GLenum coord, GLenum pname, const GLfloat *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, const GLfloat *params) = nullptr;
    if (auto call = resolve(&f, "glTexGenfvOES")) call(coord, pname, params);
}

GL_API void GL_APIENTRY glTexGeniOES(GLenum coord, GLenum pname, GLint param) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLint param) = nullptr;
    if (auto call = resolve(&f, "glTexGeniOES")) call(coord, pname, param);
}

GL_API void GL_APIENTRY glTexGenivOES(GLenum coord, GLenum pname, const GLint *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, const GLint *params) = nullptr;
    if (auto call = resolve(&f, "glTexGenivOES")) call(coord, pname, params);
}

GL_API void GL_APIENTRY glTexGenxOES(GLenum coord, GLenum pname, GLfixed param) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, GLfixed param) = nullptr;
    if (auto call = resolve(&f, "glTexGenxOES")) call(coord, pname, param);
}

GL_API void GL_APIENTRY glTexGenxvOES(GLenum coord, GLenum pname, const GLfixed *params) {
    static void (GL_APIENTRY *f)(GLenum coord, GLenum pname, const GLfixed *params) = nullptr;
    if (auto call = resolve(&f, "glTexGenxvOES")) call(coord, pname, params);
}
// Android's libGLESv1_CM adds these: the pointer calls with the length of
// the array behind the pointer, for its own checks. The host's have none, so
// the length goes.
void glColorPointerBounds(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glColorPointer(size, type, stride, pointer);
}

void glNormalPointerBounds(GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glNormalPointer(type, stride, pointer);
}

void glTexCoordPointerBounds(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glTexCoordPointer(size, type, stride, pointer);
}

void glVertexPointerBounds(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glVertexPointer(size, type, stride, pointer);
}

void glPointSizePointerOESBounds(GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glPointSizePointerOES(type, stride, pointer);
}

void glMatrixIndexPointerOESBounds(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glMatrixIndexPointerOES(size, type, stride, pointer);
}

void glWeightPointerOESBounds(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, GLsizei count) {
    (void)count;
    glWeightPointerOES(size, type, stride, pointer);
}

}  // extern "C"
