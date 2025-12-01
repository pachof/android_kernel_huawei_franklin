/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 * Description: this file contains the reused oeminfo id,
 *              and common macros and structs just for reused oeminfo or for reused oeminfo id data.
 * Author: wangsiwen
 * Create: 2019-3-7
 */

#ifndef INCLUDE_OEMINFO_REUSED_CONFIG_COMMON_H
#define INCLUDE_OEMINFO_REUSED_CONFIG_COMMON_H

/* OEMINFO_REUSED_512_BYTE_LEN means the size of the reused unit */
#define OEMINFO_REUSED_512_BYTE_LEN 512
/* OEMINFO_REUSED_128_BYTE_LEN means the size of the reused unit */
#define OEMINFO_REUSED_128_BYTE_LEN 128
/* OEMINFO_REUSED_192_BYTE_LEN means the size of the reused unit */
#define OEMINFO_REUSED_192_BYTE_LEN 192
/* OEMINFO_REUSED_96_BYTE_LEN means the size of the reused unit */
#define OEMINFO_REUSED_96_BYTE_LEN 96

/* the max reused unit size is 2K, unnecessary > 2048 Byte */
#define OEMINFO_MAX_REUSED_UNIT_SIZE 2048

#define OEMINFO_8K_UNIT_512_MAX_INDEX 15
#define OEMINFO_8K_UNIT_128_MAX_INDEX 63
#define OEMINFO_8K_UNIT_192_MAX_INDEX 42
#define OEMINFO_8K_UNIT_96_MAX_INDEX 85
#define OEMINFO_SUBID_VALID 1
#define OEMINFO_SUBID_INVALID 0

#define OEMINFO_8K_VALID_LEN                  (8 * 1024)

typedef enum {
    ENCRYPT_TYPE_NONE = 0,
    ENCRYPT_TYPE_AES128_CBC,
    ENCRYPT_TYPE_HMAC_SHA256,
} EncryptEnumType;

struct OeminfoIdReusedStruct {
    unsigned int subIdNumber; /* subid numberi from 0 */
    unsigned int offset; /* the beginning offset of subIdNumber */
    unsigned int subidValidFlg;
    unsigned int encryptType;
};

struct OeminfoReusedTableStruct {
    unsigned int oeminfoId; /* the original oeminfo id */
    unsigned int oeminfoIdSize; /* the size of the original oeminfo id */
    unsigned int subIdMax; /* the max subid number */
    unsigned int sizeOfReusedUnit; /* the reused size of each unit, such as 128 256 512 1024 2048 */
    unsigned int subTblSize; /* the number of struct OeminfoIdReusedStruct elements in reusedInfo tbl */
    struct OeminfoIdReusedStruct *reusedInfo; /* the reused struct infomation */
};

struct OeminfoReusedParasStruct {
    unsigned int oeminfoIdSize; /* the size of the original oeminfo id */
    unsigned int subIdOffset; /* the beginning offset relating to the begining of the oeminfo id */
    unsigned int sizeOfReusedUnit; /* the max size of each unit, such as 512 1024 2048 */
    unsigned int validOffset; /* the beginning offset of subIdNumber */
    unsigned int validAge;
    unsigned int invalidOffset; /* the beginning offset of subIdNumber */
    unsigned int encryptType; /* mark the encryt algorithm or no */
};

/* when add this tbl, subIdNumber should be pre_defined as enum type, max index is 15 */
static struct OeminfoIdReusedStruct g_id203ReusedParaTbl[] = {
    {
        0,
        0 * OEMINFO_REUSED_512_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    },
};

/* when add this tbl, subIdNumber should be pre_defined as enum type, max index is 63 */
static struct OeminfoIdReusedStruct g_id204ReusedParaTbl[] = {
    {
        (unsigned int)OEMINFO_REUSED_SMITHMOD,
        0 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_TAGFILE_FLAG_TYPE,
        1 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_REUSED_ALS_CAL_FACTOR,
        2 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_NFC_LOCKSCREEN_WIFICONTAG_ENABLE,
        3 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)POWEROFF_BY_USER_FLAG,
        4 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_VERSION_MAGIC,
        5 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_SAFEMODE_STATE,
        6 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_MSPES_CONFIG_FLAG,
        7 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_RECOVERY_UI_ROTATION,
        8 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_QRCODE_DATA,
        9 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_RT_TESTITEM_MAIN,
        10 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_RT_TESTITEM_SUB,
        11 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_RECOVERY_PRODUCT_PARAM,
        12 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_USB_TYPE,
        13 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_SKYTONE_SWITCH,
        14 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        15,
        15 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_INVALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_00,
        16 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_01,
        17 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_02,
        18 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_03,
        19 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_04,
        20 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_05,
        21 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_06,
        22 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_07,
        23 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_08,
        24 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_CAMERA_EEPROM_HEADER_09,
        25 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        0,
        26 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        0,
        27 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        0,
        28 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    }, {
        (unsigned int)OEMINFO_SNCODE,
        29 * OEMINFO_REUSED_128_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    },
};

static struct OeminfoIdReusedStruct g_id210ReusedParaTbl[] = {
    {
        0,
        0 * OEMINFO_REUSED_192_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    },
};

static struct OeminfoIdReusedStruct g_id211ReusedParaTbl[] = {
    {
        0,
        0 * OEMINFO_REUSED_96_BYTE_LEN,
        (unsigned int)OEMINFO_SUBID_VALID,
        (unsigned int)ENCRYPT_TYPE_NONE
    },
};

#define REUSED_OEMINFO_ID_203_TBL_LEN    ((int)(sizeof(g_id203ReusedParaTbl) / sizeof(g_id203ReusedParaTbl[0])))
#define REUSED_OEMINFO_ID_204_TBL_LEN    ((int)(sizeof(g_id204ReusedParaTbl) / sizeof(g_id204ReusedParaTbl[0])))
#define REUSED_OEMINFO_ID_210_TBL_LEN    ((int)(sizeof(g_id210ReusedParaTbl) / sizeof(g_id210ReusedParaTbl[0])))
#define REUSED_OEMINFO_ID_211_TBL_LEN    ((int)(sizeof(g_id211ReusedParaTbl) / sizeof(g_id211ReusedParaTbl[0])))

#endif
