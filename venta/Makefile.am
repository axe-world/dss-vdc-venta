ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = vdc-venta
vdc_venta_SOURCES = main.c network.c configuration.c vdsd.c util.c icons.c venta.h incbin.h

vdc_venta_CFLAGS = \
    $(PTHREAD_CFLAGS) \
    $(LIBCONFIG_CFLAGS) \
    $(JSONC_CFLAGS) \
    $(CURL_CFLAGS) \
    $(LIBDSVDC_CFLAGS) \
    $(LIBDSUID_CFLAGS)

vdc_venta_LDADD = \
    $(PTHREAD_LIBS) \
    $(LIBCONFIG_LIBS) \
    $(JSONC_LIBS) \
    $(CURL_LIBS) \
    $(LIBDSVDC_LIBS) \
    $(LIBDSUID_LIBS)
