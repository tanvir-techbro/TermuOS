#include "tlib_bundle.h"
#include "exec.h"
#include "../proc/process.h"
#include "../mm/vmm.h"
#include "../user/userspace.h"
#include "../ipc/port.h"
#include "../lib/printf.h"
#include <stdint.h>

// ── tlib_bundle_launch ────────────────────────────────────────────────────────
//
// Steps:
//   1. Validate that the app was loaded
//   2. Create a kernel process_t
//   3. ELF-load the entry binary into the process's pagemap
//   4. Register the app's declared ports in the namespace under
//      \App\<bundle_id>\<port_name>
//   5. Switch pagemap and jump to userspace
//      (does not return on success)
// ────────────────────────────────────────────────────────────────────────────

// Tiny helper: build a string of the form prefix + suffix into dst[max].
static void tl_build_path(char *dst, int max,
                          const char *prefix, const char *suffix)
{
  int i = 0, j = 0;
  while (prefix[j] && i < max - 1)
    dst[i++] = prefix[j++];
  j = 0;
  while (suffix[j] && i < max - 1)
    dst[i++] = suffix[j++];
  dst[i] = '\0';
}

int tlib_bundle_launch(tlib_app_t *app)
{
  if (!app || !app->loaded)
  {
    kprintf("tlib: launch called on unloaded app\n");
    return -1;
  }

  tlib_manifest_t *m = &app->manifest;

  // ── 1. create process ─────────────────────────────────────────────────────
  process_t *proc = proc_create(m->name);
  if (!proc)
  {
    kprintf("tlib: could not create process for '%s'\n", m->name);
    return -1;
  }

  // ── 2. load ELF into process pagemap ──────────────────────────────────────
  uint64_t entry = 0;
  if (exec_load(app->entry_path, proc, &entry) != 0)
  {
    kprintf("tlib: exec_load failed for '%s'\n", app->entry_path);
    proc_exit(proc, -1);
    return -1;
  }

  // ── 3. register declared ports in namespace ───────────────────────────────
  // Creates \App\<bundle_id>\ then one port per ports[] entry.
  // Ports are registered now so other apps can discover them by name
  // before the new process has even run its first instruction.
  {
    ob_mkdir("\\App");
    char ns_dir[128];
    tl_build_path(ns_dir, sizeof(ns_dir), "\\App\\", m->bundle_id);
    ob_mkdir(ns_dir);

    for (int i = 0; i < m->port_count; i++)
    {
      port_t *p = port_create(m->ports[i]);
      if (!p)
      {
        kprintf("tlib: failed to create port '%s'\n", m->ports[i]);
        // non-fatal: continue with remaining ports
        continue;
      }

      // insert into \App\<bundle_id>\<port_name>
      char ns_port[192];
      int dlen = 0;
      while (ns_dir[dlen])
        dlen++;
      int si = 0;
      while (ns_dir[si] && si < (int)sizeof(ns_port) - 1)
      {
        ns_port[si] = ns_dir[si];
        si++;
      }
      ns_port[si++] = '\\';
      const char *pn = m->ports[i];
      int pi2 = 0;
      while (pn[pi2] && si + pi2 < (int)sizeof(ns_port) - 1)
      {
        ns_port[si + pi2] = pn[pi2];
        pi2++;
      }
      ns_port[si + pi2] = '\0';

      ob_insert(ns_port, p->ob_header);
      kprintf("tlib: registered port '%s'\n", ns_port);
    }
  }

  // ── 4. switch pagemap and jump ────────────────────────────────────────────
  kprintf("tlib: launching '%s' (pid %u) entry=0x%x\n",
          m->name, proc->pid, (uint32_t)entry);

  tlib_set_perm_mask(m->perm_mask);
  vmm_switch(proc->pagemap);
  jump_userspace(entry, EXEC_USER_STACK_TOP);

  // jump_userspace does not return on success
  kprintf("tlib: jump_userspace returned — launch failed\n");
  return -1;
}
