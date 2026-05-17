#pragma once
#include <stdint.h>

// ─── App descriptor ───────────────────────────────────────────────────────────
//
// Each app registers itself with the desktop by filling out an app_t.
// icon_bmp points to raw 24-bit BMP data for a 32x32 icon.
// launch() is called when the user clicks the icon in the taskbar.

#define APP_ICON_W  32
#define APP_ICON_H  32

typedef struct {
    const char    *name;        // display name (shown as tooltip, not drawn yet)
    const uint8_t *icon_bmp;    // raw BMP data for 32x32 icon, or NULL for text fallback
    void         (*launch)(void); // called on click
} app_t;