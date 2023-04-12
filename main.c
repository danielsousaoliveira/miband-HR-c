/**
 * @name Heart Rate Monitor for Mi Band 6 and 7
 * @file main.c
 * @author Daniel Oliveira
 * @brief Main function to connect with the device and start notification loop.
 * @date 2023-04-04
 * 
 * @copyright Copyright (c) 2023 Daniel Oliveira
 * 
 */

#include <stdio.h>
#include <signal.h>
#include <gattlib.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <glib.h>
#include "ecdh.h"
#include "band.h"

// Initialize global main loop
GMainLoop *loop;

/**
 * @brief Signal handler for keyboard interrupt (Ctrl+C).
 * 
 * This function is called when the program receives a keyboard interrupt signal.
 * It quits the application's main loop.
 *
 * @param sig The signal received.
 */
void handle_sigint(int sig)
{
    if (sig == SIGINT)
    {
        printf("Keyboard interrupt received. Quitting...\n");
        if (loop)
        {
            g_main_loop_quit(loop);
        }
    }
}

/**
 * @brief Periodically queries the heart rate from the device.
 * 
 * This function pings the heart rate monitor and requests a new measurement.
 * It is intended to be used as a callback for glib's event loop.
 *
 * @param data The BLEDevice instance passed as user data.
 * @return gboolean Returns G_SOURCE_CONTINUE to keep the event source active.
 */
gboolean notification_query(gpointer data)
{

    BLEDevice *device = (BLEDevice *)data;
    ping_heart_rate(device);

    return G_SOURCE_CONTINUE;
}

/**
 * @brief Main function.
 * 
 * Starts the connection with the device, adds main loop callback,
 * starts the main loop and waits for keyboard interrupts to clean up.
 *  
 */
int main()
{
    const char *mac_address = MAC_ADDRESS;
    const int band_type = atoi(BAND_TYPE);

    signal(SIGINT, handle_sigint);
    loop = g_main_loop_new(NULL, FALSE);

    // Create and connect BLEDevice.
    BLEDevice *device = ble_device_create(mac_address, band_type);
    if (!device)
    {
        printf("Failed to connect to the device.\n");
        return 1;
    }

    // Set callback function for the loop.
    guint timeout_id = g_timeout_add(10000, notification_query, (gpointer)device);

    // Enable notifications of chunked tranfer to start authentication.
    enable_notifications_chunked(device);

    // Starts glib main event loop.
    g_main_loop_run(loop);

    // Plot recorded heart rate.
    plot_heart_rate(device);

    // Clean up.
    g_source_remove(timeout_id);
    ble_device_destroy(device);

    return 0;
}
