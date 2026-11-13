/* PRO-X, GPL-3.0-or-later. See docs/wpa2-support.md. */
#include <pspkernel.h>
#include "main.h"
#include "printk.h"
#include "utils.h"
#include "prox_network.h"
#include "wpa2.h"
#include "wpa2_variant_661.h"

static int config_ready;
static int enabled = 1;

void prox_wpa2_init_config(void)
{
    enum ProxNetworkStatus status;
    if (config_ready) return;
    status = prox_network_read(&enabled);
    config_ready = 1;
    printk("PRO-X network config: status=%d enabled=%d\n",status,enabled);
    (void)status; /* Normal builds compile printk out. */
}

enum Wpa2Result prox_wpa2_module(SceModule2 *mod)
{
    Wpa2Module view;
    enum Wpa2Result result;
    unsigned int i;
    if (!config_ready) {
        printk("PRO-X WPA2: result=%d module=%s\n",WPA2_CONFIG_NOT_READY,mod->modname);
        return WPA2_CONFIG_NOT_READY;
    }
    view.name = mod->modname;
    view.firmware = psp_fw_version;
    view.text = mod->text_addr;
    view.text_size = mod->text_size;
    view.data_size = mod->data_size;
    view.bss_size = mod->bss_size;
    view.nsegment = mod->nsegment;
    for (i = 0; i < 4; ++i) {
        view.segment[i].address = mod->segmentaddr[i];
        view.segment[i].size = mod->segmentsize[i];
    }
    /* Content/layout identification is shared across consoles and instances.
     * Unknown or partially patched modules are still rejected before writing. */
    result = prox_wpa2_apply(&view,enabled,&prox_wpa2_variant_661);
    if (result == WPA2_APPLIED) sync_cache();
    printk("PRO-X WPA2: result=%d module=%s\n",result,mod->modname);
    return result;
}
