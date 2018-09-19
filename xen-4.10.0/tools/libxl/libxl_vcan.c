#include "libxl_internal.h"

static int libxl__device_vcan_setdefault(libxl__gc *gc, uint32_t domid,
                                           libxl_device_vcan *vcan,
                                           bool hotplug)
{
    return libxl__resolve_domid(gc, vcan->backend_domname,
                                &vcan->backend_domid);
}

static int libxl__device_from_vcan(libxl__gc *gc, uint32_t domid,
                                     libxl_device_vcan *vcan,
                                     libxl__device *device)
{
   device->backend_devid   = vcan->devid;
   device->backend_domid   = vcan->backend_domid;
   device->backend_kind    = LIBXL__DEVICE_KIND_VCAN;
   device->devid           = vcan->devid;
   device->domid           = domid;
   device->kind            = LIBXL__DEVICE_KIND_VCAN;

   return 0;
}

static int libxl__vcan_from_xenstore(libxl__gc *gc, const char *libxl_path,
                                       libxl_devid devid,
                                       libxl_device_vcan *vcan)
{
    const char *be_path;
    int rc;

    vcan->devid = devid;
    rc = libxl__xs_read_mandatory(gc, XBT_NULL,
                                  GCSPRINTF("%s/backend", libxl_path),
                                  &be_path);
    if (rc) return rc;

    return libxl__backendpath_parse_domid(gc, be_path, &vcan->backend_domid);
}

static void libxl__update_config_vcan(libxl__gc *gc,
                                        libxl_device_vcan *dst,
                                        libxl_device_vcan *src)
{
    dst->devid = src->devid;
}

static int libxl_device_vcan_compare(libxl_device_vcan *d1,
                                       libxl_device_vcan *d2)
{
    return COMPARE_DEVID(d1, d2);
}

static void libxl__device_vcan_add(libxl__egc *egc, uint32_t domid,
                                     libxl_device_vcan *vcan,
                                     libxl__ao_device *aodev)
{
    libxl__device_add_async(egc, domid, &libxl__vcan_devtype, vcan, aodev);
}

static int libxl__set_xenstore_vcan(libxl__gc *gc, uint32_t domid,
                                      libxl_device_vcan *vcan,
                                      flexarray_t *back, flexarray_t *front,
                                      flexarray_t *ro_front)
{
				  
	flexarray_append_pair(ro_front, "dev-id",
                          GCSPRINTF("%d", (10*domid + vcan->devid)));
	
    flexarray_append_pair(back, "dev-id",
                          GCSPRINTF("%d", (10*domid + vcan->devid)));
    flexarray_append_pair(back, "tx-permissions",
                          GCSPRINTF("%s", vcan->tx));
    flexarray_append_pair(back, "rx-permissions",
                          GCSPRINTF("%s", vcan->rx));

    return 0;
}


int libxl_device_vcan_getinfo(libxl_ctx *ctx, uint32_t domid,
                                libxl_device_vcan *vcan,
                                libxl_vcaninfo *info)
{
    GC_INIT(ctx);
    char *libxl_path, *dompath, *devpath;
    char *val;
    int rc;

    libxl_vcaninfo_init(info);
    dompath = libxl__xs_get_dompath(gc, domid);
    info->devid = vcan->devid;

    devpath = GCSPRINTF("%s/device/vcan/%d", dompath, info->devid);
    libxl_path = GCSPRINTF("%s/device/vcan/%d",
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

	
    
    rc = 0;

out:
     GC_FREE;
     return rc;
}

int libxl_devid_to_device_vcan(libxl_ctx *ctx, uint32_t domid,
                                 int devid, libxl_device_vcan *vcan)
{
    GC_INIT(ctx);

    libxl_device_vcan *vcans = NULL;
    int n, i;
    int rc;

    libxl_device_vcan_init(vcan);

    vcans = libxl__device_list(gc, &libxl__vcan_devtype, domid, &n);


    if (!vcans) { rc = ERROR_NOTFOUND; goto out; }

    for (i = 0; i < n; ++i) {
        if (devid == vcans[i].devid) {
            libxl_device_vcan_copy(ctx, vcan, &vcans[i]);
            rc = 0;
            goto out;
        }
    }

    rc = ERROR_NOTFOUND;

out:

    if (vcans)
        libxl__device_list_free(&libxl__vcan_devtype, vcans, n);

    GC_FREE;
    return rc;
}

LIBXL_DEFINE_DEVICE_ADD(vcan)
static LIBXL_DEFINE_DEVICES_ADD(vcan)
LIBXL_DEFINE_DEVICE_REMOVE(vcan)
static LIBXL_DEFINE_UPDATE_DEVID(vcan, "vcan")
LIBXL_DEFINE_DEVICE_LIST(vcan)

DEFINE_DEVICE_TYPE_STRUCT(vcan,
    .update_config = (device_update_config_fn_t)libxl__update_config_vcan,
    .from_xenstore = (device_from_xenstore_fn_t)libxl__vcan_from_xenstore,
    .set_xenstore_config = (device_set_xenstore_config_fn_t)
                           libxl__set_xenstore_vcan
);

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
