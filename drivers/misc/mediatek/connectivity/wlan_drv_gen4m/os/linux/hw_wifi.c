/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: Adapte mtk chip interface
 * Author: cw949445
 * Create: 2020-09-14
 */

#include "hw_wifi.h"
#include "debug.h"
#include "gl_cfg80211.h"

const void *get_wificfg_filename_header(void)
{
    struct device_node *node = NULL;

    node = of_find_node_by_path("/huawei_wifi_info");
    if (node == NULL) {
        DBGLOG(INIT, INFO, "get_wificfg_filename_header:Node is not available.\n");
        return NULL;
    }
    DBGLOG(INIT, INFO, "get_wificfg_filename_header:Node is vail.\n");

    return of_get_property(node, "wifi_cfg_type", NULL);
}
