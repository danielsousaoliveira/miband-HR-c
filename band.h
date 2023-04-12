/**
 * @name Heart Rate Monitor for Mi Band 6 and 7
 * @headerfile band.h
 * @author Daniel Oliveira
 * @brief Structure to handle Mi Band services and characteristics & functions declaration.
 * @date 2023-04-04
 *
 * @copyright Copyright (c) 2023 Daniel Oliveira
 *
 */

#include <gattlib.h>

/**
 * @brief Mi Band structure containing necessary information for BLE communication, services, characteristics and heart rate data.
 *
 */
typedef struct
{
    gatt_connection_t *connection;
    gattlib_primary_service_t *services;
    gattlib_characteristic_t *characteristics;
    gattlib_characteristic_t characteristicChunkedR;
    gattlib_characteristic_t characteristicChunkedW;
    gattlib_characteristic_t characteristicFetch;
    gattlib_characteristic_t characteristicActivityData;
    gattlib_characteristic_t characteristicTime;
    gattlib_characteristic_t characteristicHrControl;
    gattlib_characteristic_t characteristicHrMeasure;
    gattlib_characteristic_t characteristicAlert;

    int32_t **hrHist;
    uint8_t *authKey;
    uint8_t *privateKey;
    uint8_t *publicKey;
    uint8_t *secretKey;
    uint8_t lastSequenceNumber;
    uint8_t pointer;
    uint8_t expectedBytes;
    uint8_t handle;

    int serviceCount;
    int characteristicCount;
    int hrCount;
    int histSize;

} BLEDevice;

/**
 * @brief Create a Mi Band instance (BLEDevice) and connect to the device with the given MAC address.
 * @param mac_address The MAC address of the device to connect to.
 * @param band_type The type of the Mi Band.
 * @return A pointer to the created BLEDevice instance, or NULL if the connection failed.
 *
 * This function allocates memory for a new BLEDevice instance, connects to the device using the
 * provided MAC address, initializes its fields, and discovers primary services and characteristics.
 * If the connection fails, the function frees the memory and returns NULL.
 */
BLEDevice *ble_device_create(const char *mac_address, const int band_type);

/**
 * @brief Clean up memory and disconnect with the device.
 * @param device The BLEDevice instance to disconnect and clean up.
 * 
 * This function disconnects the BLE device, frees the allocated memory for the device's
 * properties, and deallocates the BLEDevice instance.
 * 
 */
void ble_device_destroy(BLEDevice *device);

/**
 * @brief Split value in different packets (chunks) to write in the chunked transfer characteristic
 * @param connection The GATT connection to use for writing.
 * @param char_uuid The UUID of the chunked transfer characteristic.
 * @param type The type of chunked transfer.
 * @param handle The handle to use for the transfer.
 * @param data The data to write.
 * @param data_length The length of the data to write.
 * 
 * This function sends the provided data in chunks to the target characteristic
 * of the connected BLE device. The data is split into multiple chunks if its
 * size exceeds the maximum transmission unit (MTU).
 * 
 */
void write_chunked_value(gatt_connection_t *connection, uuid_t *char_uuid, uint8_t type, uint8_t handle, uint8_t *data, size_t data_length);

/**
 * @brief Prepare a public-private key pair using ECDH key agreement.
 * @param device The BLEDevice instance.
 * @return A pointer to the public key.
 * 
 * This function generates a random private key, generates the ECDH key pair, and
 * creates a final array containing the generated public key, and a prefix to identify.
 * 
 */
uint8_t *prepare_pub_key(BLEDevice *device);

/**
 * @brief Read authentication key from a text file and format it as anbyte array.
 * @return A pointer to the authentication key.
 * 
 * This function reads the authentication key from a given text file and
 * creates a byte array from the read data.
 * 
 */
uint8_t *prepare_auth_key();

/**
 * @brief Encrypt data using AES CBC.
 * @param key The key used for encryption.
 * @param in The input data to be encrypted.
 * @param out The output buffer to store the encrypted data.
 * @param length The length of the input data.
 * 
 * This function takes input data and encrypts it using AES-CBC with the provided key.
 *
 */
void encrypt_aes_cbc(const uint8_t *key, const uint8_t *in, uint8_t *out, int length);

/**
 * @brief Start continuous heart rate measurement on the device.
 * @param device The BLEDevice instance.
 * 
 * This function sends commands to start the heart rate measurement
 * and enables notifications for heart rate measurements.
 * 
 */
void start_hr_measure(BLEDevice *device);

/**
 * @brief Send a query to the device to continue heart rate measurement.
 * @param device The BLEDevice instance.
 * 
 * This function sends commands to query the heart rate measurement and keep the continuous measurement.
 */
void ping_heart_rate(BLEDevice *device);

/**
 * @brief Plot the heart rate data collected from the device.
 * @param device The BLEDevice instance.
 * 
 * This function creates a pipe to the gnuplot and plots the heart rate data
 * that has been stored in the BLEDevice instance.
 */
void plot_heart_rate(BLEDevice *device);

/**
 * @brief Send an alert notification to the connected BLE device. (call)
 * 
 * @param device A pointer to the BLEDevice structure representing the connected device.
 */
void send_alert(BLEDevice *device);

/**
 * @brief Enable notifications for chunked data transfer characteristic.
 * @param device The BLEDevice instance.
 * 
 * This function enables notifications for chunked transfer and
 * registers the notification event handler.
 */
void enable_notifications_chunked(BLEDevice *device);

/**
 * @brief Callback function to be called when enabling notifications has succeeded.
 * @param device The BLEDevice instance.
 * @param uuid The UUID of the characteristic for which notifications were enabled.
 * 
 * This function processes the event when the notifications for a characteristic
 * are enabled. If the characteristic that sent the notification is related to the,
 * chunked transfer, it sends the first part of the authentication.
 */
void characteristic_enable_notifications_succeeded(BLEDevice *device, const uuid_t uuid);

/**
 * @brief Callback function to be called when a notification is received.
 * @param uuid The UUID of the updated characteristic.
 * @param value The updated value.
 * @param value_length The length of the updated value.
 * @param user_data A pointer to user-defined data, in this case the BLEDevice instance.
 * 
 * This function processes the updated characteristic value and handles the
 * authentication process as well as the heart rate data reception and storage.
 *
 */
void characteristic_value_updated(const uuid_t *uuid, const uint8_t *value, size_t value_length, void *user_data);