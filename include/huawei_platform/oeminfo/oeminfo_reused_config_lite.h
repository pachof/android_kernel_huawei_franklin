/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2020. All rights reserved.
 * Description: this file contains macros and structs just for reused oeminfo
 *              or for reused oeminfo id data in version lite.
 * Author: wangyukai
 * Create: 2018-8-10
 */

#ifndef OEMINFO_OPERATION_LITE_OEMINFO_REUSED_CONFIG_LITE_H
#define OEMINFO_OPERATION_LITE_OEMINFO_REUSED_CONFIG_LITE_H

static const struct OeminfoReusedTableStruct g_reusedOeminfoTable[] = {
    {
        (unsigned int)OEMINFO_NONEWP_REUSED_ID_551_8K_VALID_SIZE_448_BYTE,
        (unsigned int)OEMINFO_8K_VALID_LEN,
        (unsigned int)OEMINFO_8K_UNIT_512_MAX_INDEX,
        (unsigned int)OEMINFO_REUSED_512_BYTE_LEN,
        (unsigned int)REUSED_OEMINFO_ID_203_TBL_LEN,
        g_id203ReusedParaTbl
    }, {
        (unsigned int)OEMINFO_NONEWP_REUSED_ID_552_8K_VALID_SIZE_64_BYTE,
        (unsigned int)OEMINFO_8K_VALID_LEN,
        (unsigned int)OEMINFO_8K_UNIT_128_MAX_INDEX,
        (unsigned int)OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)REUSED_OEMINFO_ID_204_TBL_LEN,
        g_id204ReusedParaTbl
    }, {
        (unsigned int)OEMINFO_NONEWP_REUSED_ID_555_8K_VALID_SIZE_128_BYTE,
        (unsigned int)OEMINFO_8K_VALID_LEN,
        (unsigned int)OEMINFO_8K_UNIT_192_MAX_INDEX,
        (unsigned int)OEMINFO_REUSED_192_BYTE_LEN,
        (unsigned int)REUSED_OEMINFO_ID_210_TBL_LEN,
        g_id210ReusedParaTbl
    }, {
        (unsigned int)OEMINFO_NONEWP_REUSED_ID_556_8K_VALID_SIZE_32_BYTE,
        (unsigned int)OEMINFO_8K_VALID_LEN,
        (unsigned int)OEMINFO_8K_UNIT_96_MAX_INDEX,
        (unsigned int)OEMINFO_REUSED_96_BYTE_LEN,
        (unsigned int)REUSED_OEMINFO_ID_211_TBL_LEN,
        g_id211ReusedParaTbl
    },
};

#define REUSED_OEMINFO_TABLE_LEN    (sizeof(g_reusedOeminfoTable) / sizeof(g_reusedOeminfoTable[0]))

#endif