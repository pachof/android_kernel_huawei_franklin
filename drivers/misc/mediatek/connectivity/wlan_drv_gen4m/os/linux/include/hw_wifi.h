

#ifndef _HW_WIFI_H
#define _HW_WIFI_H

#include "gl_os.h"
#include "gl_typedef.h"

/* adapt mtk wifi.cfg for multi-product*/
#define HW_WIFICFG_MULTI_PRODUCT  1
#define WIFI_CFG_NAME_DEFAULT "wifi.cfg"
#define WIFI_CFG_NAME_MAX_LEN 128

extern const void *get_wificfg_filename_header(void);
#endif /* _HW_WIFI_H */
