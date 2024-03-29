/**
 * @name Heart Rate Monitor for Mi Band 6 and 7
 * @headerfile uuid.h
 * @author Daniel Oliveira
 * @brief Mi Band Universally Unique Identifiers
 * @date 2023-04-04
 * 
 * @copyright Copyright (c) 2023 Daniel Oliveira
 * 
 */

static char *SERVICE_MIBAND1 = "0xfee0";
static char *SERVICE_MIBAND2 = "0xfee1";
static char *SERVICE_ALERT = "0x1802";
static char *SERVICE_ALERT_NOTIFICATION = "0x1811";
static char *SERVICE_HEART_RATE = "0x180d";
static char *SERVICE_DEVICE_INFO = "0x180a";

static char *CHARACTERISTIC_HZ = "00000002-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_SENSOR = "00000001-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_AUTH = "00000009-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_HEART_RATE_MEASURE = "0x2a37";
static char *CHARACTERISTIC_HEART_RATE_CONTROL = "0x2a39";
static char *CHARACTERISTIC_ALERT = "0x2a46";
static char *CHARACTERISTIC_BATTERY = "00000006-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_STEPS = "00000007-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_LE_PARAMS = "0000ff09-0000-1000-8000-00805f9b34fb";
static char *CHARACTERISTIC_CONFIGURATION = "00000003-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_DEVICEEVENT = "00000010-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_CURRENT_TIME = "0x2a2b";
static char *CHARACTERISTIC_AGE = "00002a80-0000-1000-8000-00805f9b34fb";
static char *CHARACTERISTIC_USER_SETTINGS = "00000008-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_ACTIVITY_DATA = "00000005-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_FETCH = "00000004-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_CHUNKED_TRANSFER = "00000020-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_CHUNKED_TRANSFER_WRITE = "00000016-0000-3512-2118-0009af100700";
static char *CHARACTERISTIC_CHUNKED_TRANSFER_READ = "00000017-0000-3512-2118-0009af100700";
