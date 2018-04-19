#include "libxl_internal.h"

static int libxl__device_vgpio_setdefault(libxl__gc *gc, uint32_t domid,
                                           libxl_device_vgpio *vgpio,
                                           bool hotplug)
{
    return libxl__resolve_domid(gc, vgpio->backend_domname,
                                &vgpio->backend_domid);
}

static int libxl__device_from_vgpio(libxl__gc *gc, uint32_t domid,
                                     libxl_device_vgpio *vgpio,
                                     libxl__device *device)
{
   device->backend_devid   = vgpio->devid;
   device->backend_domid   = vgpio->backend_domid;
   device->backend_kind    = LIBXL__DEVICE_KIND_VGPIO;
   device->devid           = vgpio->devid;
   device->domid           = domid;
   device->kind            = LIBXL__DEVICE_KIND_VGPIO;

   return 0;
}

static int libxl__vgpio_from_xenstore(libxl__gc *gc, const char *libxl_path,
                                       libxl_devid devid,
                                       libxl_device_vgpio *vgpio)
{
    const char *be_path;
    int rc;

    vgpio->devid = devid;
    rc = libxl__xs_read_mandatory(gc, XBT_NULL,
                                  GCSPRINTF("%s/backend", libxl_path),
                                  &be_path);
    if (rc) return rc;

    return libxl__backendpath_parse_domid(gc, be_path, &vgpio->backend_domid);
}

static void libxl__update_config_vgpio(libxl__gc *gc,
                                        libxl_device_vgpio *dst,
                                        libxl_device_vgpio *src)
{
    dst->devid = src->devid;
}

static int libxl_device_vgpio_compare(libxl_device_vgpio *d1,
                                       libxl_device_vgpio *d2)
{
    return COMPARE_DEVID(d1, d2);
}

static void libxl__device_vgpio_add(libxl__egc *egc, uint32_t domid,
                                     libxl_device_vgpio *vgpio,
                                     libxl__ao_device *aodev)
{
    libxl__device_add_async(egc, domid, &libxl__vgpio_devtype, vgpio, aodev);
}

static int libxl__set_xenstore_vgpio(libxl__gc *gc, uint32_t domid,
                                      libxl_device_vgpio *vgpio,
                                      flexarray_t *back, flexarray_t *front,
                                      flexarray_t *ro_front)
{

    flexarray_append_pair(back, "output-pins",
                          GCSPRINTF("%s", vgpio->output_pins));
	flexarray_append_pair(back, "input-pins",
                          GCSPRINTF("%s", vgpio->input_pins));
	flexarray_append_pair(back, "irq-pins",
					  GCSPRINTF("%s", vgpio->irq_pins));
					  
	
    flexarray_append_pair(ro_front, "output-pins",
                          GCSPRINTF("%s", vgpio->output_pins));
	flexarray_append_pair(ro_front, "input-pins",
                          GCSPRINTF("%s", vgpio->input_pins));
	flexarray_append_pair(ro_front, "irq-pins",
					  GCSPRINTF("%s", vgpio->irq_pins));
    return 0;
}


int libxl_device_vgpio_getinfo(libxl_ctx *ctx, uint32_t domid,
                                libxl_device_vgpio *vgpio,
                                libxl_vgpioinfo *info)
{
    GC_INIT(ctx);
    char *libxl_path, *dompath, *devpath;
    char *val;
    int rc;

    libxl_vgpioinfo_init(info);
    dompath = libxl__xs_get_dompath(gc, domid);
    info->devid = vgpio->devid;

    devpath = GCSPRINTF("%s/device/vgpio/%d", dompath, info->devid);
    libxl_path = GCSPRINTF("%s/device/vgpio/%d",
                           libxl__xs_libxl_path(gc, domid),
                           info->devid);
    info->backend = xs_read(ctx->xsh, XBT_NULL,
                            GCSPRINTF("%s/backend", libxl_path),
                            NULL);
    if (!info->backend) { rc = ERROR_FAIL; goto out; }

    rc = libxl__backendpath_parse_domid(gc, info->backend, &info->backend_id);
    if (rc) goto out;

    val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/state", devpath));
    info->state = val ? strtoul(val, NULL, 10) : -1;

    info->frontend = xs_read(ctx->xsh, XBT_NULL,
                             GCSPRINTF("%s/frontend", libxl_path),
                             NULL);
    info->frontend_id = domid;

	printf("libxl_device_vgpio_getinfo...\n");
    val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/output-pins", devpath));
    info->output_pins = val;
    
    val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/input-pins", devpath));
    info->input_pins = val;
    
    val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/irq-pins", devpath));
    info->irq_pins = val;
	printf("OK!\n");

    rc = 0;

out:
     GC_FREE;
     return rc;
}

int libxl_devid_to_device_vgpio(libxl_ctx *ctx, uint32_t domid,
                                 int devid, libxl_device_vgpio *vgpio)
{
	printf("libxl_devid_to_device_vgpio():\n");
	printf("domid: %d\ndevid: %d\n", domid, devid);
    GC_INIT(ctx);

    libxl_device_vgpio *vgpios = NULL;
    int n, i;
    int rc;

    libxl_device_vgpio_init(vgpio);

    vgpios = libxl__device_list(gc, &libxl__vgpio_devtype, domid, &n);

	printf(" n_vgpios: %d\n", n);

    if (!vgpios) { rc = ERROR_NOTFOUND; goto out; }

    for (i = 0; i < n; ++i) {
        if (devid == vgpios[i].devid) {
            libxl_device_vgpio_copy(ctx, vgpio, &vgpios[i]);
            rc = 0;
            goto out;
        }
    }

    rc = ERROR_NOTFOUND;

out:

    if (vgpios)
        libxl__device_list_free(&libxl__vgpio_devtype, vgpios, n);

    GC_FREE;
    return rc;
}

LIBXL_DEFINE_DEVICE_ADD(vgpio)
static LIBXL_DEFINE_DEVICES_ADD(vgpio)
LIBXL_DEFINE_DEVICE_REMOVE(vgpio)
static LIBXL_DEFINE_UPDATE_DEVID(vgpio, "vgpio")
LIBXL_DEFINE_DEVICE_LIST(vgpio)

DEFINE_DEVICE_TYPE_STRUCT(vgpio,
    .update_config = (device_update_config_fn_t)libxl__update_config_vgpio,
    .from_xenstore = (device_from_xenstore_fn_t)libxl__vgpio_from_xenstore,
    .set_xenstore_config = (device_set_xenstore_config_fn_t)
                           libxl__set_xenstore_vgpio
);

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
