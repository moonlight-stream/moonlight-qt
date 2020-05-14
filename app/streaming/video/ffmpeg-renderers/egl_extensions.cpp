// vim: noai:ts=4:sw=4:softtabstop=4:expandtab
#define GL_GLEXT_PROTOTYPES

#include "renderer.h"

#include <SDL_egl.h>

static QStringList egl_get_extensions(EGLDisplay dpy) {
    const auto EGLExtensionsStr = eglQueryString(dpy, EGL_EXTENSIONS);
    if (!EGLExtensionsStr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unable to get EGL extensions");
        return QStringList();
    }
    return QString(EGLExtensionsStr).split(" ");
}

EGLExtensions::EGLExtensions(EGLDisplay dpy) :
    m_Extensions(egl_get_extensions(dpy))
{}

bool EGLExtensions::isSupported(const QString &extension) const {
    return m_Extensions.contains(extension);
}
