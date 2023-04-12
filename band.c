/**
 * @name Heart Rate Monitor for Mi Band 6 and 7
 * @file band.c
 * @author Daniel Oliveira
 * @brief Structure to handle Mi Band services, characteristics and functions.
 * @date 2023-04-04
 *
 * @copyright Copyright (c) 2023 Daniel Oliveira
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <gattlib.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include "band.h"
#include "ecdh.h"
#include "uuid.h"

// Global time valu to store heart rate notification timestamps in seconds
time_t initial_timestamp;

/**
 * @brief Create a Mi Band instance (BLEDevice) and connect to the device with the given MAC address.
 */
BLEDevice *ble_device_create(const char *mac_address, const int band_type)
{
    // Allocate memory for the BLEDevice structure and initialize it.
    BLEDevice *device = malloc(sizeof(BLEDevice));

    // Establish a connection to the device with the provided MAC address.
    device->connection = gattlib_connect(NULL, mac_address, GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
    if (device->connection == NULL)
    {
        printf("Failed to connect to the device.\n");
        free(device);
        return NULL;
    }

    // Initialize the properties of the BLEDevice structure.
    device->handle = 0;
    device->lastSequenceNumber = 0;
    device->pointer = 0;
    device->expectedBytes = 0;
    device->privateKey = (uint8_t *)malloc(ECC_PRV_KEY_SIZE);
    device->publicKey = (uint8_t *)malloc(ECC_PUB_KEY_SIZE);
    device->secretKey = (uint8_t *)malloc(ECC_PUB_KEY_SIZE);
    device->authKey = prepare_auth_key();

    // Initialize heart rate array and allocate memory for it.
    device->hrCount = 0;
    device->histSize = 1000;
    device->hrHist = malloc(device->histSize * sizeof(int32_t *));
    for (int i = 0; i < device->histSize; ++i)
    {
        device->hrHist[i] = malloc(2 * sizeof(int32_t));
    }

    // Discover the primary services and characteristics of the connected device.
    gattlib_discover_primary(device->connection, &device->services, &device->serviceCount);
    gattlib_discover_char(device->connection, &device->characteristics, &device->characteristicCount);

    // Assign the discovered characteristics to their corresponding properties in the BLEDevice structure.
    for (int i = 0; i < device->characteristicCount; i++)
    {
        char uuid_str[MAX_LEN_UUID_STR + 1];
        gattlib_uuid_to_string(&device->characteristics[i].uuid, uuid_str, sizeof(uuid_str));

        if (strcmp(uuid_str, CHARACTERISTIC_CHUNKED_TRANSFER_READ) == 0)
        {
            device->characteristicChunkedR = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_CHUNKED_TRANSFER_WRITE) == 0)
        {
            device->characteristicChunkedW = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_FETCH) == 0)
        {
            device->characteristicFetch = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_ACTIVITY_DATA) == 0)
        {
            device->characteristicActivityData = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_CURRENT_TIME) == 0)
        {
            device->characteristicTime = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_HEART_RATE_CONTROL) == 0)
        {
            device->characteristicHrControl = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_HEART_RATE_MEASURE) == 0)
        {
            device->characteristicHrMeasure = device->characteristics[i];
        }
        else if (strcmp(uuid_str, CHARACTERISTIC_ALERT) == 0)
        {
            device->characteristicAlert = device->characteristics[i];
        }
    }

    return device;
}

/**
 * @brief Clean up memory and disconnect with the device.
 */
void ble_device_destroy(BLEDevice *device)
{
    // Disconnect the BLE device.
    gattlib_disconnect(device->connection);

    // Free the allocated memory for the device's properties.
    free(device->hrHist);
    free(device->services);
    free(device->characteristics);

    // Deallocate the BLEDevice instance.
    free(device);
}

/**
 * @brief Split value in differente packets to write in the chunked transfer characteristic
 */
void write_chunked_value(gatt_connection_t *connection, uuid_t *char_uuid, uint8_t type, uint8_t handle, uint8_t *data, size_t data_length)
{

    size_t remaining = data_length;
    int count = 0;
    int header_size = 11;
    const int mMTU = 23;

    // Iterate through the data, sending it in chunks
    while (remaining > 0)
    {
        const int MAX_CHUNKLENGTH = mMTU - 3 - header_size;
        const size_t copybytes = (remaining < MAX_CHUNKLENGTH) ? remaining : MAX_CHUNKLENGTH;
        uint8_t chunk[copybytes + header_size];

        int flags = 0;

        // If this is the first chunk, include the total data length and transfer type.
        if (count == 0)
        {
            flags |= 0x01;
            chunk[5] = data_length & 0xff;
            chunk[6] = (data_length >> 8) & 0xff;
            chunk[7] = (data_length >> 16) & 0xff;
            chunk[8] = (data_length >> 24) & 0xff;
            chunk[9] = type & 0xff;
            chunk[10] = (type >> 8) & 0xff;
        }

        // If this is the last chunk, set the end flag.
        if (remaining <= MAX_CHUNKLENGTH)
        {
            flags |= 0x06;
        }

        // Set up the header for the chunk.
        chunk[0] = 0x03;
        chunk[1] = flags;
        chunk[2] = 0;
        chunk[3] = handle;
        chunk[4] = count;

        // Copy the data for this chunk.
        memcpy(chunk + header_size, data + data_length - remaining, copybytes);

        // Write the chunk to the specified characteristic.
        gattlib_write_char_by_uuid(connection, char_uuid, chunk, copybytes + header_size);

        // Update remaining data and header size.
        remaining -= copybytes;
        header_size = 5;
        count++;
    }
}

/**
 * @brief Prepare a public-private key pair using ECDH key agreement.
 */
uint8_t *prepare_pub_key(BLEDevice *device)
{

    uint8_t random_key[ECC_PRV_KEY_SIZE];

    // Generate random bytes for the private key.
    if (RAND_bytes(random_key, sizeof(random_key)) != 1)
    {
        fprintf(stderr, "Error generating random bytes\n");
    }

    // Generate the ECDH key pair.
    if (ecdh_generate_keys(device->publicKey, device->privateKey) != 1)
    {
        fprintf(stderr, "Error generating public key\n");
    }

    // Prepare the final array containing the generated public key and a prefix to identify.
    uint8_t prefix[] = {0x04, 0x02, 0x00, 0x02};
    size_t prefix_len = sizeof(prefix) / sizeof(prefix[0]);

    size_t total_len = prefix_len + ECC_PUB_KEY_SIZE;
    uint8_t *final_array = (uint8_t *)malloc(total_len * sizeof(uint8_t));

    memcpy(final_array, prefix, prefix_len);
    memcpy(final_array + prefix_len, device->publicKey, ECC_PUB_KEY_SIZE);

    return final_array;
}

/**
 * @brief Read authentication key from a text file and format it as an byte array.
 */
uint8_t *prepare_auth_key()
{

    char auth_key[33];
    FILE *file = fopen(AUTH_KEY_FILE, "r");

    // Handle file opening errors.
    if (file == NULL)
    {
        fprintf(stderr, "Error: Could not open file \n");
        exit(1);
    }

    // Read the authentication key from the file.
    if (fgets(auth_key, 33, file) == NULL)
    {
        fprintf(stderr, "Error: Could not read auth key from file \n");
        fclose(file);
        exit(1);
    }

    // Remove newline character from the authentication key, if present.
    auth_key[strcspn(auth_key, "\n")] = '\0';

    // Convert the authentication key from a hex string to a byte array.
    uint8_t *keyArray = (uint8_t *)malloc(16 * sizeof(uint8_t));
    for (size_t i = 0; i < strlen(auth_key) / 2; i++)
    {
        sscanf(auth_key + 2 * i, "%2hhx", &keyArray[i]);
    }

    return keyArray;
}

/**
 * @brief Encrypt data using AES CBC.
 */
void encrypt_aes_cbc(const uint8_t *key, const uint8_t *in, uint8_t *out, int length)
{
    AES_KEY aes_key;
    uint8_t iv[AES_BLOCK_SIZE] = {0};
    AES_set_encrypt_key(key, 128, &aes_key);
    AES_cbc_encrypt(in, out, length, &aes_key, iv, AES_ENCRYPT);
}

/**
 * @brief Start continuous heart rate measurement on the device.
 */
void start_hr_measure(BLEDevice *device)
{

    uint8_t data[] = {0x15, 0x01, 0x01};
    size_t data_len = sizeof(data) / sizeof(data[0]);
    uint8_t data2[] = {0x14, 0x00, 0x01};
    size_t data_len2 = sizeof(data2) / sizeof(data2[0]);

    // Start notifications for the heart rate measurement characteristic.
    int ret2 = gattlib_notification_start(device->connection, &device->characteristicHrMeasure.uuid);

    if (ret2 != GATTLIB_SUCCESS)
    {
        printf("Failed to start notifications for heart rate: %d\n", ret2);
    }

    // Start counting time.
    initial_timestamp = time(NULL);

    // Start continuous measurement.
    gattlib_write_char_by_uuid(device->connection, &device->characteristicHrControl.uuid, data, data_len);

    // Set measurement interval.
    gattlib_write_char_by_uuid(device->connection, &device->characteristicHrControl.uuid, data2, data_len2);
}

/**
 * @brief Send a query to the device to continue heart rate measurement.
 */
void ping_heart_rate(BLEDevice *device)
{

    uint8_t data[] = {0x15, 0x01, 0x01};
    size_t data_len = sizeof(data) / sizeof(data[0]);
    uint8_t data2[] = {0x14, 0x00, 0x01};
    size_t data_len2 = sizeof(data2) / sizeof(data2[0]);

    // Start continuous measurement.
    gattlib_write_char_by_uuid(device->connection, &device->characteristicHrControl.uuid, data, data_len);

    // Set measurement interval.
    gattlib_write_char_by_uuid(device->connection, &device->characteristicHrControl.uuid, data2, data_len2);
}

/**
 * @brief Plot the heart rate data collected from the device.
 */
void plot_heart_rate(BLEDevice *device)
{

    // Open a pipe to gnuplot
    FILE *gnuplot_pipe = popen("gnuplot -persistent", "w");
    if (!gnuplot_pipe)
    {
        fprintf(stderr, "Error: could not open a pipe to gnuplot\n");
        return;
    }

    // Configure the plot
    fprintf(gnuplot_pipe, "set title 'Heart Rate vs. Time'\n");
    fprintf(gnuplot_pipe, "set xlabel 'Time (s)'\n");
    fprintf(gnuplot_pipe, "set ylabel 'Heart Rate (bpm)'\n");
    fprintf(gnuplot_pipe, "plot '-' with linespoints linetype 1 linecolor 'blue', '' with points pointtype 6 lc rgb 'red'\n");

    // Send data points to gnuplot (line)
    for (int i = 0; i < device->hrCount; i++)
    {
        fprintf(gnuplot_pipe, "%d %d\n", device->hrHist[i][0], device->hrHist[i][1]);
    }

    fprintf(gnuplot_pipe, "e\n");

    // Send data points to gnuplot (points)
    for (int i = 0; i < device->hrCount; i++)
    {
        fprintf(gnuplot_pipe, "%d %d\n", device->hrHist[i][0], device->hrHist[i][1]);
    }

    fprintf(gnuplot_pipe, "e\n");

    // Finish the plot
    fflush(gnuplot_pipe);
}

/**
 * @brief Send an alert notification to the connected BLE device. (call)
 */
void send_alert(BLEDevice *device)
{

    uint8_t data[] = {0x03, 0x01, 0x0a, 0x0a, 0x0a};
    size_t data_len = sizeof(data) / sizeof(data[0]);

    // Send call notification alert.
    printf("Sending call notification to the band\n");
    gattlib_write_char_by_uuid(device->connection, &device->characteristicAlert.uuid, data, data_len);
}

/**
 * @brief Enable notifications for chunked data transfer characteristic.
 */
void enable_notifications_chunked(BLEDevice *device)
{

    // Register the notification event handler.
    gattlib_register_notification(device->connection, (gattlib_event_handler_t)characteristic_value_updated, (void *)device);

    // Start notifications for the chunked transfer characteristic.
    int ret = gattlib_notification_start(device->connection, &device->characteristicChunkedR.uuid);

    if (ret == GATTLIB_SUCCESS)
    {
        // Set function to be called when the notifications are enabled.
        characteristic_enable_notifications_succeeded(device, device->characteristicChunkedW.uuid);
    }
    else
    {
        printf("Failed to start notifications for char_chunked: %d\n", ret);
    }
}

/**
 * @brief Callback function to be called when enabling notifications has succeeded.
 */
void characteristic_enable_notifications_succeeded(BLEDevice *device, uuid_t uuid)
{
    // Convert char UUID for comparison purpose.
    char uuid_str[MAX_LEN_UUID_STR + 1];
    gattlib_uuid_to_string(&uuid, uuid_str, sizeof(uuid_str));

    // If the characteristic is related to chunked transfer, send 1st authentication part.
    if (strcmp(uuid_str, CHARACTERISTIC_CHUNKED_TRANSFER_WRITE) == 0)
    {
        printf("Sending 1st Auth Part \n");
        uint8_t *auth = prepare_pub_key(device);
        write_chunked_value(device->connection, &uuid, 0x82, device->handle, auth, 52);
    }
}

/**
 * @brief Callback function to be called when a notification is received.
 */
void characteristic_value_updated(const uuid_t *uuid, const uint8_t *value, size_t value_length, void *user_data)
{

    BLEDevice *device = (BLEDevice *)user_data;

    char uuid_str[MAX_LEN_UUID_STR + 1];
    gattlib_uuid_to_string(uuid, uuid_str, sizeof(uuid_str));

    // Handle chunked transfer characteristic value updates.
    if (strcmp(uuid_str, CHARACTERISTIC_CHUNKED_TRANSFER_READ) == 0)
    {
        // Check the header.
        if (value_length > 1 && value[0] == 0x03)
        {
            // Check the authentication packets.
            uint8_t sequence_number = value[4];
            size_t header_size = 0;

            if (sequence_number == 0 &&
                value[9] == 0x82 &&
                value[10] == 0x00 &&
                value[11] == 0x10 &&
                value[12] == 0x04 &&
                value[13] == 0x01)
            {
                printf("1st authentication part completed\n");
                device->pointer = 0;
                header_size = 14;
                device->expectedBytes = value[5] - 3;
            }
            else if (sequence_number > 0)
            {
                if (sequence_number != device->lastSequenceNumber + 1)
                {
                    printf("Unexpected sequence number\n");
                }
                header_size = 5;
            }
            else if (value[9] == 0x82 &&
                     value[10] == 0x00 &&
                     value[11] == 0x10 &&
                     value[12] == 0x05 &&
                     value[13] == 0x01)
            {
                printf("Successfully authenticated\n");
                start_hr_measure(device);
            }
            else
            {
                printf("Unhandled characteristic change\n");
            }

            // Check if pointer matches the expected bytes for the 1st auth part.
            uint8_t bytes_to_copy = value_length - header_size;

            device->pointer += bytes_to_copy;
            device->lastSequenceNumber = sequence_number;

            if (device->pointer == device->expectedBytes)
            {
                const uint8_t *remoteRandom = value + header_size;
                const uint8_t *remotePublic = value + header_size + 16;
                uint8_t finalSharedSessionAES[16];
                uint8_t out1[16];
                uint8_t out2[16];
                uint8_t command[33];

                // Create shared ECDH key using private key and the device public key.
                if (ecdh_shared_secret(device->privateKey, remotePublic, device->secretKey) != 1)
                {
                    fprintf(stderr, "Error in key\n");
                }

                for (int i = 0; i < 16; i++)
                {
                    finalSharedSessionAES[i] = device->secretKey[i + 8] ^ device->authKey[i];
                }

                // Encrypt data to send.
                encrypt_aes_cbc(device->authKey, remoteRandom, out1, 16);
                encrypt_aes_cbc(finalSharedSessionAES, remoteRandom, out2, 16);

                // Format data according to auth logic.
                command[0] = 0x05;
                memcpy(command + 1, out1, 16);
                memcpy(command + 17, out2, 16);

                printf("Sending 2nd Auth Part\n");
                write_chunked_value(device->connection, &device->characteristicChunkedW.uuid, 0x82, device->handle + 1, command, 33);
            }
        }
    }

    // Handle heart rate measurement characteristic updates.
    if (strcmp(uuid_str, CHARACTERISTIC_HEART_RATE_MEASURE) == 0)
    {
        // Increment number of heart rate measures.
        device->hrCount += 1;

        // Check if the buffer needs to be resized.
        if (device->hrCount == device->histSize)
        {
            device->histSize *= 2;
            int **tempArray = realloc(device->hrHist, device->histSize * sizeof(int32_t *));
            for (int i = (device->histSize / 2); i < device->histSize; ++i)
            {
                tempArray[i] = malloc(2 * sizeof(int32_t));
            }
            if (tempArray == NULL)
            {
                printf("Error while allocating memory! \n");
                plot_heart_rate(device);
                device->hrCount = 0;
                device->histSize = 1000;
                device->hrHist = realloc(device->hrHist, device->histSize * sizeof(int32_t *));
                for (int i = 0; i < device->histSize; ++i)
                {
                    device->hrHist[i] = malloc(2 * sizeof(int32_t));
                }
            }
            else
            {
                device->hrHist = tempArray;
            }
        }

        // Read the heart rate value.
        size_t len = 2;
        int32_t result = 0;
        for (size_t i = 0; i < len; i++)
        {
            result = (result << 8) | value[i];
        }
        printf("Heart Rate Value: %i \n", result);

        // Store the value and the time at which it was received.
        device->hrHist[device->hrCount - 1][0] = (int32_t)(time(NULL) - initial_timestamp);
        device->hrHist[device->hrCount - 1][1] = result;

        // Calculate mean value to send alert in case heart rate is decreasing.
        double sum = 0;
        double mean = 0;

        for (int i = 0; i < device->hrCount; i++)
        {
            sum += device->hrHist[i][1];
        }

        mean = sum / device->hrCount;

        // Send alert to the band.
        if ((device->hrCount > 60) && (result < mean - 10))
        {
            send_alert(device);
        }
    }
}