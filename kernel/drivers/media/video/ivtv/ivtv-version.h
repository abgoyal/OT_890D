

#ifndef IVTV_VERSION_H
#define IVTV_VERSION_H

#define IVTV_DRIVER_NAME "ivtv"
#define IVTV_DRIVER_VERSION_MAJOR 1
#define IVTV_DRIVER_VERSION_MINOR 4
#define IVTV_DRIVER_VERSION_PATCHLEVEL 0

#define IVTV_VERSION __stringify(IVTV_DRIVER_VERSION_MAJOR) "." __stringify(IVTV_DRIVER_VERSION_MINOR) "." __stringify(IVTV_DRIVER_VERSION_PATCHLEVEL)
#define IVTV_DRIVER_VERSION KERNEL_VERSION(IVTV_DRIVER_VERSION_MAJOR,IVTV_DRIVER_VERSION_MINOR,IVTV_DRIVER_VERSION_PATCHLEVEL)

#endif
