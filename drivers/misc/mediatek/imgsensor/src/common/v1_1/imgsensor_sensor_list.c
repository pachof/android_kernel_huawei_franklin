/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This file should be the same as
 *    mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_SENSOR_LIST
	gimgsensor_sensor_list[MAX_NUM_OF_SUPPORT_SENSOR] = {
/*IMX*/
/*macro*/
/*OV (OmniVision)*/
#if defined(OV02A10_SUNWIN)
{ OV02A10_SUNWIN_SENSOR_ID, SENSOR_DRVNAME_OV02A10_SUNWIN, OV02A10_SUNWIN_SensorInit },
#endif
#if defined(OV48B2Q_LUXVISIONS)
{ OV48B2Q_LUXVISIONS_SENSOR_ID, SENSOR_DRVNAME_OV48B2Q_LUXVISIONS, OV48B2Q_LUXVISIONS_SensorInit },
#endif
#if defined(OV16A1Q_OFILM)
{ OV16A1Q_OFILM_SENSOR_ID, SENSOR_DRVNAME_OV16A1Q_OFILM, OV16A1Q_OFILM_SensorInit },
#endif
/*S5K*/
#if defined(S5K3P9_SUNNY)
    { S5K3P9_SUNNY_SENSOR_ID, SENSOR_DRVNAME_S5K3P9_SUNNY, S5K3P9_SUNNY_SensorInit },
#endif
/*HI*/
#if defined(HI846_OFILM)
{ HI846_OFILM_SENSOR_ID, SENSOR_DRVNAME_HI846_OFILM, HI846_OFILM_SensorInit },
#endif
#if defined(HI846_SUNNY)
{ HI846_SUNNY_SENSOR_ID, SENSOR_DRVNAME_HI846_SUNNY, HI846_SUNNY_SensorInit },
#endif
#if defined(OV8856_FOXCONN)
{ OV8856_FOXCONN_SENSOR_ID, SENSOR_DRVNAME_OV8856_FOXCONN, OV8856_FOXCONN_SensorInit },
#endif
/*MT*/
/*GC*/
#if defined(GC2375_BYD)
	{ GC2375_BYD_SENSOR_ID, SENSOR_DRVNAME_GC2375_BYD, GC2375_BYD_SensorInit },
#endif
/*SP*/
/*SIV*/
/*PAS (PixArt Image)*/
/*Panasoic*/
/*Toshiba*/
/*Others*/

	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};

/* e_add new sensor driver here */

