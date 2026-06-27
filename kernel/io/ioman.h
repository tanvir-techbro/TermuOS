#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../ob/object.h"

/* ── IRP major function codes ──────────────────────────────────────────── */
#define IRP_MJ_READ         0
#define IRP_MJ_WRITE        1
#define IRP_MJ_IOCTL        2
#define IRP_MJ_CREATE       3
#define IRP_MJ_CLOSE        4
#define IRP_MJ_MAX          5

/* ── IRP status ────────────────────────────────────────────────────────── */
#define IRP_STATUS_PENDING   0
#define IRP_STATUS_SUCCESS   1
#define IRP_STATUS_ERROR     2

/* ── forward declarations ──────────────────────────────────────────────── */
typedef struct irp         irp_t;
typedef struct device_obj  device_obj_t;
typedef struct driver_obj  driver_obj_t;

/* ── completion callback ───────────────────────────────────────────────── */
typedef void (*irp_completion_t)(irp_t *irp, void *ctx);

/* ── IRP ───────────────────────────────────────────────────────────────── */
struct irp {
    uint8_t          major;        /* IRP_MJ_* */
    uint32_t         ioctl_code;   /* for IRP_MJ_IOCTL */

    void            *buffer;       /* data buffer */
    uint32_t         length;       /* buffer length */
    uint32_t         offset;       /* byte/sector offset */

    uint32_t         bytes_transferred;
    int              status;       /* IRP_STATUS_* */

    device_obj_t    *device;       /* target device */
    irp_completion_t completion;   /* called on finish */
    void            *completion_ctx;
};

/* ── Driver dispatch table ─────────────────────────────────────────────── */
typedef int (*irp_handler_t)(device_obj_t *dev, irp_t *irp);

struct driver_obj {
    char            name[32];
    irp_handler_t   dispatch[IRP_MJ_MAX];
    object_header_t *ob_header;
};

/* ── Device object ─────────────────────────────────────────────────────── */
struct device_obj {
    char            name[32];       /* e.g. "ata0", "kbd" */
    driver_obj_t   *driver;
    void           *extension;      /* driver-private data */
    object_header_t *ob_header;
};

/* ── API ───────────────────────────────────────────────────────────────── */
void          ioman_init(void);
driver_obj_t *ioman_create_driver(const char *name);
device_obj_t *ioman_create_device(driver_obj_t *driver, const char *name, void *extension);
int           ioman_send_irp(device_obj_t *dev, irp_t *irp);
device_obj_t *ioman_get_device(const char *name);  /* lookup \Device\<name> */

/* helpers to build common IRPs */
irp_t irp_read (void *buf, uint32_t len, uint32_t offset, irp_completion_t cb, void *ctx);
irp_t irp_write(const void *buf, uint32_t len, uint32_t offset, irp_completion_t cb, void *ctx);
