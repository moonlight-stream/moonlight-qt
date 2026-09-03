/*
 * libva-probe: loadability oracle for the AppImage AppRun.
 *
 * Moonlight and the bundled FFmpeg import libva through symbols carrying
 * VA_API_* ELF version nodes. Whether the libva installed on the host can
 * satisfy those requirements cannot be inferred portably from file names or
 * loader caches, so this program embeds the same DT_NEEDED entries and
 * versioned symbol references as those binaries without ever calling into
 * them: either the dynamic loader resolves everything and main() runs (the
 * host libva is usable), or the exec itself fails and AppRun must fall back
 * to the build-environment libva staged next to this binary.
 *
 * scripts/build-appimage.sh verifies at build time that this file references
 * every VA_API_* node required by the binaries shipped in the AppImage, so
 * it stays automatically in sync with the libva usage there.
 */

#include <stddef.h>

#include <va/va.h>
#include <va/va_x11.h>

/* Address references are enough: they produce relocations against the
 * versioned symbols, which is what makes the loader check the host's
 * VA_API_* version nodes. The functions must never actually be called.
 * volatile keeps the table (and its relocations) alive through -O2. */
static void *volatile requirements[] = {
    (void *)vaCreateSurfaces, /* libva.so.2, VA_API_0.33.0 */
    (void *)vaMapBuffer2,     /* libva.so.2, 2.21+ */
    (void *)vaGetDisplay,     /* libva-x11.so.2 */
};

int main(void)
{
    return requirements[0] == NULL;
}
