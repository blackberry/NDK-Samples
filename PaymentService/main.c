/*
 * Copyright (c) 2011-2012 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bps/bps.h>
#include <bps/paymentservice.h>
#include <bps/navigator.h>
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>
static screen_context_t screen_ctx;
static screen_window_t screen_win;

/**
 * Use the PID to set the window group id.
 */
static const char *
get_window_group_id()
{
    static char s_window_group_id[16] = "";

    if (s_window_group_id[0] == '\0') {
        snprintf(s_window_group_id, sizeof(s_window_group_id), "%d", getpid());
    }

    return s_window_group_id;
}

/**
 * Set up a basic screen, so that the navigator will
 * send window state events when the window state changes.
 *
 * @return @c EXIT_SUCCESS or @c EXIT_FAILURE
 */
int
setup_screen()
{
    if (screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        return EXIT_FAILURE;
    }

    if (screen_create_window(&screen_win, screen_ctx) != 0) {
        screen_destroy_context(screen_ctx);
        return EXIT_FAILURE;
    }

    if (screen_create_window_group(screen_win, get_window_group_id()) != 0) goto fail;

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) != 0) goto fail;

    if (screen_create_window_buffers(screen_win, 1) != 0) goto fail;

    screen_buffer_t buff;
    if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void*)&buff) != 0) goto fail;

    int buffer_size[2];
    if (screen_get_buffer_property_iv(buff, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size) != 0) goto fail;

    int attribs[1] = {SCREEN_BLIT_END};
    if (screen_fill(screen_ctx, buff, attribs) != 0) goto fail;

    int dirty_rects[4] = {0, 0, buffer_size[0], buffer_size[1]};
    if (screen_post_window(screen_win, buff, 1, (const int*)dirty_rects, 0) != 0) goto fail;

    return EXIT_SUCCESS;

fail:
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return EXIT_FAILURE;
}


/**
 * Handle the failure case for either event. Print the error information.
 */
void failureCommon(bps_event_t *event)
{
    if (event == NULL) {
        fprintf(stderr, "Invalid event.\n");
        return;
    }

    unsigned request_id = paymentservice_event_get_request_id(event);
    int error_id = paymentservice_event_get_error_id(event);
    const char* error_text = paymentservice_event_get_error_text(event);

    fprintf(stderr, "Payment System error. Request ID: %d  Error ID: %d  Text: %s\n",
            request_id, error_id, error_text ? error_text : "N/A");
}

/**
 * Upon successful completion of a purchase, print a string containing
 * information about the digital good that was purchased.
 */
void onPurchaseSuccess(bps_event_t *event)
{
    if (event == NULL) {
        fprintf(stderr, "Invalid event.\n");
        return;
    }

    unsigned request_id = paymentservice_event_get_request_id(event);
    const char* date = paymentservice_event_get_date(event, 0);
    const char* digital_good = paymentservice_event_get_digital_good_id(event, 0);
    const char* digital_sku = paymentservice_event_get_digital_good_sku(event, 0);
    const char* license_key = paymentservice_event_get_license_key(event, 0);
    const char* metadata = paymentservice_event_get_metadata(event, 0);
    const char* purchase_id = paymentservice_event_get_purchase_id(event, 0);

    fprintf(stderr, "Purchase success. Request Id: %d\n Date: %s\n DigitalGoodID: %s\n SKU: %s\n License: %s\n Metadata: %s\n PurchaseId: %s\n\n",
        request_id,
        date ? date : "N/A",
        digital_good ? digital_good : "N/A",
        digital_sku ? digital_sku : "N/A",
        license_key ? license_key : "N/A",
        metadata ? metadata : "N/A",
        purchase_id ? purchase_id : "N/A");
}

/**
 * On successful completion of a get existing purchases request,
 * print the existing purchases.
 */
void onGetExistingPurchasesSuccess(bps_event_t *event)
{
    if (event == NULL) {
        fprintf(stderr, "Invalid event.\n");
        return;
    }

    unsigned request_id = paymentservice_event_get_request_id(event);
    int purchases = paymentservice_event_get_number_purchases(event);

    fprintf(stderr, "Get existing purchases success. Request ID: %d\n", request_id);
    fprintf(stderr, "Number of existing purchases: %d\n", purchases);
    fprintf(stderr, "Existing purchases:\n");

    int i = 0;
    for (i = 0; i<purchases; i++) {
        const char* date = paymentservice_event_get_date(event, i);
        const char* digital_good = paymentservice_event_get_digital_good_id(event, i);
        const char* digital_sku = paymentservice_event_get_digital_good_sku(event, i);
        const char* license_key = paymentservice_event_get_license_key(event, i);
        const char* metadata = paymentservice_event_get_metadata(event, i);
        const char* purchase_id = paymentservice_event_get_purchase_id(event, i);

        fprintf(stderr, "  Date: %s  PurchaseID: %s  DigitalGoodID: %s  SKU: %s  License: %s  Metadata: %s\n",
            date ? date : "N/A",
            purchase_id ? purchase_id : "N/A",
            digital_good ? digital_good : "N/A",
            digital_sku ? digital_sku : "N/A",
            license_key ? license_key : "N/A",
            metadata ? metadata : "N/A");
    }
}

/**
 * A sample application that demonstrates the BlackBerry(R) 10 Native SDK APIs
 * for making in-app purchases. The sample sets the connection mode to local,
 * allows the purchase of a digital good by using the swipe down gesture, and
 * displays any existing purchases.
 */
int
main(int argc, char *argv[])
{
    int exit_application = 0;

    /*
     * Before we can listen for events from the BlackBerry(R) 10 OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    /*
     * Initialize the screen so that the window group Id is properly set, to allow
     * the Payment Service dialogs to be displayed.
     */
    if (setup_screen() != EXIT_SUCCESS) {
        fprintf(stderr, "Unable to initialize screen.");
        exit(0);
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event), and as a
     * convenient way to trigger a purchase request (NAVIGATOR_SWIPE_DOWN).
     * We request PaymentService events so we can be notified when the payment service
     * responds to our requests/queries.
     */
    navigator_request_events(0);
    paymentservice_request_events(0);

    /*
     * Set the Payment Service connection mode to local. This allows us to
     * test the API without the need to contact the AppWorld nor payment servers.
     */
    paymentservice_set_connection_mode(true);

    /*
     * Create a set of purchase parameters, which describe the digital good
     * to be purchased and the application the goods are associated with.
     */
    const char* digital_good_id = "Digital-Good-1-ID";
    const char* digital_good_name = "Sample Digital Good 1";
    const char* digital_good_sku = "SAMPLE_DIGITAL_GOOD_SKU_1";
    const char* metadata = "Sample purchase metadata";
    const char* purchase_app_icon = "http://www.rim.com/products/appworld_3col.jpg";
    const char* purchase_app_name = "Payment Service Sample App";

    /*
     * Define a request ID to hold the returned value from the purchase request.
     */
    unsigned request_id = 0;

    /*
     * initiate the application with a purchase of the sample digital good.
     */
    if (paymentservice_purchase_request(digital_good_id, digital_good_sku, digital_good_name,
            metadata, purchase_app_name, purchase_app_icon, get_window_group_id(), &request_id) != BPS_SUCCESS) {
        fprintf(stderr, "Error: purchase request failed.\n");
    }

    /*
     * Process Payment Service and Navigator events until we receive a NAVIGATOR_EXIT event.
     */
    while (!exit_application) {
        /*
         * Using a negative timeout (-1) in the call to bps_get_event(...)
         * ensures that we don't busy wait by blocking until an event is
         * available.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            /*
             * If it is a Payment Service event, determine the response code
             * and handle the event accordingly.
             */
            if (bps_event_get_domain(event) == paymentservice_get_domain()) {
                if (SUCCESS_RESPONSE == paymentservice_event_get_response_code(event)) {
                    if (PURCHASE_RESPONSE == bps_event_get_code(event)) {
                        onPurchaseSuccess(event);
                        unsigned request_id = 0;
                        if (paymentservice_get_existing_purchases_request(false, get_window_group_id(), &request_id) != BPS_SUCCESS) {
                            fprintf(stderr, "Error: get existing purchases failed.\n");
                        }
                    } else
                        onGetExistingPurchasesSuccess(event);
                } else {
                    failureCommon(event);
                }
            }

            /*
             * If it is a NAVIGATOR_EXIT event then set the exit_application
             * flag so the application will stop processing events, clean up
             * and exit.
             *
             * If it is a NAVIGATOR_SWIPE_DOWN event, initiate the purchase of
             * the sample digital good.
             */
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (NAVIGATOR_EXIT == bps_event_get_code(event)) {
                    exit_application = 1;
                } else if (NAVIGATOR_SWIPE_DOWN == bps_event_get_code(event)) {
                    if (paymentservice_purchase_request(digital_good_id, digital_good_sku, digital_good_name,
                            metadata, purchase_app_name, purchase_app_icon, get_window_group_id(), &request_id) != BPS_SUCCESS) {
                        fprintf(stderr, "Error: purchase request failed.\n");
                    }
                }
            }
        }
    }

    /*
     * Clean up the BPS infrastructure and exit
     */
    bps_shutdown();
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return 0;
}
