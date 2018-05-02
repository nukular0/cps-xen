#include "libxl_internal.h"

static int libxl__device_vspi_setdefault(libxl__gc *gc, uint32_t domid,
                                           libxl_device_vspi *vspi,
                                           bool hotplug)
{
    return libxl__resolve_domid(gc, vspi->backend_domname,
                                &vspi->backend_domid);
}

static int libxl__device_from_vspi(libxl__gc *gc, uint32_t domid,
                                     libxl_device_vspi *vspi,
                                     libxl__device *device)
{
   device->backend_devid   = vspi->devid;
   device->backend_domid   = vspi->backend_domid;
   device->backend_kind    = LIBXL__DEVICE_KIND_VSPI;
   device->devid           = vspi->devid;
   device->domid           = domid;
   device->kind            = LIBXL__DEVICE_KIND_VSPI;

   return 0;
}

static int libxl__vspi_from_xenstore(libxl__gc *gc, const char *libxl_path,
                                       libxl_devid devid,
                                       libxl_device_vspi *vspi)
{
    const char *be_path;
    int rc;

    vspi->devid = devid;
    rc = libxl__xs_read_mandatory(gc, XBT_NULL,
                                  GCSPRINTF("%s/backend", libxl_path),
                                  &be_path);
    if (rc) return rc;

    return libxl__backendpath_parse_domid(gc, be_path, &vspi->backend_domid);
}

static void libxl__update_config_vspi(libxl__gc *gc,
                                        libxl_device_vspi *dst,
                                        libxl_device_vspi *src)
{
    dst->devid = src->devid;
}

static int libxl_device_vspi_compare(libxl_device_vspi *d1,
                                       libxl_device_vspi *d2)
{
    return COMPARE_DEVID(d1, d2);
}

static void libxl__device_vspi_add(libxl__egc *egc, uint32_t domid,
                                     libxl_device_vspi *vspi,
                                     libxl__ao_device *aodev)
{
    libxl__device_add_async(egc, domid, &libxl__vspi_devtype, vspi, aodev);
}

static int libxl__set_xenstore_vspi(libxl__gc *gc, uint32_t domid,
                                      libxl_device_vspi *vspi,
                                      flexarray_t *back, flexarray_t *front,
                                      flexarray_t *ro_front)
{
				  
	
    flexarray_append_pair(ro_front, "busnum",
                          GCSPRINTF("%d", vspi->busnum));
    flexarray_append_pair(ro_front, "num_cs",
                          GCSPRINTF("%d", vspi->num_cs));
    flexarray_append_pair(ro_front, "max_speed_hz",
                          GCSPRINTF("%d", vspi->max_speed_hz));

    return 0;
}


int libxl_device_vspi_getinfo(libxl_ctx *ctx, uint32_t domid,
                                libxl_device_vspi *vspi,
                                libxl_vspiinfo *info)
{
    GC_INIT(ctx);
    char *libxl_path, *dompath, *devpath;
    char *val;
    int rc;

    libxl_vspiinfo_init(info);
    dompath = libxl__xs_get_dompath(gc, domid);
    info->devid = vspi->devid;

    devpath = GCSPRINTF("%s/device/vspi/%d", dompath, info->devid);
    libxl_path = GCSPRINTF("%s/device/vspi/%d",
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

	val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/busnum", devpath));
    info->busnum = val ? strtoul(val, NULL, 10) : -1;
	val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/max_speed_hz", devpath));
    info->max_speed_hz = val ? strtoul(val, NULL, 10) : -1;
	val = libxl__xs_read(gc, XBT_NULL, GCSPRINTF("%s/num_cs", devpath));
    info->num_cs = val ? strtoul(val, NULL, 10) : -1;
    
    rc = 0;

out:
     GC_FREE;
     return rc;
}

int libxl_devid_to_device_vspi(libxl_ctx *ctx, uint32_t domid,
                                 int devid, libxl_device_vspi *vspi)
{
    GC_INIT(ctx);

    libxl_device_vspi *vspis = NULL;
    int n, i;
    int rc;

    libxl_device_vspi_init(vspi);

    vspis = libxl__device_list(gc, &libxl__vspi_devtype, domid, &n);


    if (!vspis) { rc = ERROR_NOTFOUND; goto out; }

    for (i = 0; i < n; ++i) {
        if (devid == vspis[i].devid) {
            libxl_device_vspi_copy(ctx, vspi, &vspis[i]);
            rc = 0;
            goto out;
        }
    }

    rc = ERROR_NOTFOUND;

out:

    if (vspis)
        libxl__device_list_free(&libxl__vspi_devtype, vspis, n);

    GC_FREE;
    return rc;
}

LIBXL_DEFINE_DEVICE_ADD(vspi)
static LIBXL_DEFINE_DEVICES_ADD(vspi)
LIBXL_DEFINE_DEVICE_REMOVE(vspi)
static LIBXL_DEFINE_UPDATE_DEVID(vspi, "vspi")
LIBXL_DEFINE_DEVICE_LIST(vspi)

DEFINE_DEVICE_TYPE_STRUCT(vspi,
    .update_config = (device_update_config_fn_t)libxl__update_config_vspi,
    .from_xenstore = (device_from_xenstore_fn_t)libxl__vspi_from_xenstore,
    .set_xenstore_config = (device_set_xenstore_config_fn_t)
                           libxl__set_xenstore_vspi
);

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
