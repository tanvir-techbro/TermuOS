#pragma once
#include <stdint.h>
#include <stddef.h>

// ── tlib: application bundle loader ─────────────────────────────────────────
//
// A .tapp bundle is a VFS directory with this layout:
//
//   MyApp.tapp/
//   ├── manifest.json   ← parsed by tlib_manifest_parse()
//   ├── bin/MyApp       ← entry point ELF (path from manifest "entry" field)
//   ├── lib/            ← app-private shared libraries (.tso)
//   └── res/            ← resources
//
// manifest.json schema (all string values, arrays of strings):
//   {
//     "name":        "MyApp",
//     "bundle_id":   "com.example.myapp",
//     "version":     "1.0.0",
//     "entry":       "bin/MyApp",
//     "permissions": ["ipc.send", "fs.read"],
//     "ports":       ["MyApp.main"]
//   }
//
// Known permissions:
//   ipc.send      ipc.receive
//   fs.read       fs.write
//   proc.spawn
// ────────────────────────────────────────────────────────────────────────────

#define TLIB_NAME_MAX       64
#define TLIB_BUNDLE_ID_MAX  128
#define TLIB_VERSION_MAX    32
#define TLIB_ENTRY_MAX      128
#define TLIB_MAX_PERMS      16
#define TLIB_MAX_PORTS      8
#define TLIB_PERM_LEN       32
#define TLIB_PORT_LEN       64

// permission bit-flags (stored in tlib_app_t.perm_mask)
#define TLIB_PERM_IPC_SEND    (1u << 0)
#define TLIB_PERM_IPC_RECEIVE (1u << 1)
#define TLIB_PERM_FS_READ     (1u << 2)
#define TLIB_PERM_FS_WRITE    (1u << 3)
#define TLIB_PERM_PROC_SPAWN  (1u << 4)

typedef struct
{
  char     name[TLIB_NAME_MAX];
  char     bundle_id[TLIB_BUNDLE_ID_MAX];
  char     version[TLIB_VERSION_MAX];
  char     entry[TLIB_ENTRY_MAX];           // relative path inside bundle

  char     permissions[TLIB_MAX_PERMS][TLIB_PERM_LEN];
  int      perm_count;

  char     ports[TLIB_MAX_PORTS][TLIB_PORT_LEN];
  int      port_count;

  uint32_t perm_mask;                       // resolved from permissions[]
} tlib_manifest_t;

// parsed + loaded application record
typedef struct
{
  tlib_manifest_t manifest;
  char            bundle_path[256];         // e.g. "/mnt/MyApp.tapp"
  char            entry_path[256];          // bundle_path + "/" + manifest.entry
  int             loaded;                   // 1 after tlib_bundle_load() succeeds
} tlib_app_t;

// ── API ──────────────────────────────────────────────────────────────────────

// Parse manifest.json text (null-terminated) into *out.
// Returns 0 on success, -1 on parse error.
int tlib_manifest_parse(const char *json, tlib_manifest_t *out);

// Load a .tapp bundle from the VFS path `bundle_path`.
// Reads and parses manifest.json, resolves the entry path.
// Does NOT launch a process — call tlib_bundle_launch() for that.
// Returns 0 on success, -1 on error.
int tlib_bundle_load(const char *bundle_path, tlib_app_t *out);

// Pretty-print a manifest to the kernel console (for debugging).
void tlib_manifest_dump(const tlib_manifest_t *m);

// Launch a loaded .tapp bundle as a new process.
// `app` must have been successfully passed through tlib_bundle_load() first.
// Switches to the new process's pagemap and jumps to userspace.
// Does not return on success.  Returns -1 if launch fails.
int tlib_bundle_launch(tlib_app_t *app);
