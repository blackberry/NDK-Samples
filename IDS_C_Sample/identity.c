/**
 * Integration with the IDS library for interacting with the User Identity on the device
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "identity.h"

// Globals for maintaining the provider connections for the app
static int bbid_fd = -1;
static ids_provider_t* bbid_provider = NULL;

static int profile_fd = -1;
static ids_provider_t* profile_provider = NULL;

// forward declarations for setting text in the UI
void setTitleText( const char* text );
void setStatusText( const char* text );
void setCountText( const char* text );

// Number of times the button has been pressed, stored in BlackBerry Profile so
// it is accessible across devices for the same user.
static int pressCount = 0;

/**
 * BPS will fire this callback each time the given FD has INPUT, just
 * need to call the ids_process_msg() function to handle the event.
 */
static int identity_bps_io_handler( int fd, int event, void* data ) {
    fprintf( stderr, "Got INPUT on the FD\n" );
    if( IDS_SUCCESS == ids_process_msg( fd ) ) {
        return BPS_SUCCESS;
    }

    fprintf( stderr, "Failed processing FD, no longer connected to provider\n" );

    // Handle cleanup of held values since ids_process_msg() returned non-SUCCESS and they
    // will have been released.  The application is no longer connected to the provider.
    if( fd == bbid_fd ) {
        bbid_provider = NULL;
        bbid_fd = -1;
    } else if( fd == profile_fd ) {
        profile_provider = NULL;
        profile_fd = -1;
    }
    bps_remove_fd( fd );
    return BPS_FAILURE;
}

/**
 * Starts up the IDS Library for use by the application
 *
 * @param windowGroupId The window Group Id of the application so that dialogs and login screens can be displayed
 * in the context of the application as required by BlackBerry ID for authorization/authentication by the user.
 * @return The result of the identity initialisation. EXIT_SUCCESS in success
 * case and EXIT_FAILURE if a failure occurs.
 */
int identity_initialize( const char* windowGroupId ) {

    // Setup internal library resources to get it ready to be used by the application
    ids_result_t result = ids_initialize();
    if( IDS_SUCCESS != result ) {
        fprintf( stderr, "Failed to initialise the IDS Library.  %s\n", strerror( errno ) );
        return EXIT_FAILURE;
    }

    // Setup the Window Group ID so that user interaction on behalf of the application can be performed.
    result = ids_set_option( IDS_OPT_GROUP_ID, windowGroupId );
    if( IDS_SUCCESS != result ) {
        fprintf( stderr, "Failed to set group ID.  %s\n", strerror( errno ) );
        ids_shutdown();
        return EXIT_FAILURE;
    }

    // During development, increase the default logging level of the
    // IDS Library to verbose level to output helpful progress information.
    result = ids_set_option( IDS_OPT_VERBOSITY, IDS_LOG_VERBOSE );
    if( IDS_SUCCESS != result ) {
        fprintf( stderr, "Failed to adjust logging level.  %s\n", strerror( errno ) );
        ids_shutdown();
        return EXIT_FAILURE;
    }

    // Connect your application to the actual identity provider that will
    // handle your application's requests for BlackBerry Profile information.
    result = ids_register_provider( BLACKBERRY_PROFILE_PROVIDER, &profile_provider, &profile_fd );
    if( IDS_SUCCESS != result ) {
        fprintf( stderr, "Failed to register with the BlackBerry Profile provider.  %s\n", strerror( errno ) );
        ids_shutdown();
        return EXIT_FAILURE;
    }

    // Monitor the provider fd for input as part of the application event loop and call
    // ids_process_msg() to handle any events that are detected
    if( BPS_SUCCESS != bps_add_fd( profile_fd, BPS_IO_INPUT, identity_bps_io_handler, NULL ) ) {
        fprintf( stderr, "Unable to listen to fd. %s\n", strerror( errno ) );
        ids_shutdown();
        profile_fd = -1;
        profile_provider = NULL;
        return EXIT_FAILURE;
    }

    // Connect your application to the actual identity provider that will
    // handle your application's requests for BlackBerry ID information.
    result = ids_register_provider( BLACKBERRY_ID_PROVIDER, &bbid_provider, &bbid_fd );
    if( IDS_SUCCESS != result ) {
        fprintf( stderr, "Failed to register with the BlackBerry ID provider.  %s\n", strerror( errno ) );
        bps_remove_fd( profile_fd );
        profile_fd = -1;
        profile_provider = NULL;
        ids_shutdown();
        return EXIT_FAILURE;
    }

    // Monitor the provider fd for input as part of the application event loop and call
    // ids_process_msg() to handle any events that are detected
    if( BPS_SUCCESS != bps_add_fd( bbid_fd, BPS_IO_INPUT, identity_bps_io_handler, NULL ) ) {
        fprintf( stderr, "Unable to listen to fd. %s\n", strerror( errno ) );
        ids_shutdown();
        bps_remove_fd( profile_fd );
        profile_fd = -1;
        profile_provider = NULL;
        bbid_fd = -1;
        bbid_provider = NULL;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * Release all resources held by the IDS Library.
 */
void identity_shutdown() {
    // stop monitoring fds for input
    bps_remove_fd( profile_fd );
    bps_remove_fd( bbid_fd );

    // all resources will be released by calling shutdown
    ids_shutdown();

    // the file descriptor and provider are both freed resources now, clear values
    bbid_fd = -1;
    bbid_provider = NULL;
    profile_fd = -1;
    profile_provider = NULL;
}

// set/create success callback
static void my_data_success_handler( ids_request_id_t request_id,
                                     void* cb_data ) {
    fprintf( stderr, "Success.\n" );
    setStatusText( "Successfully got profile data" );

    pressCount++;
    // update display with new pressCount
    char buffer[64];
    snprintf( buffer, sizeof( buffer ), "Pressed %d times before", pressCount );
    setCountText( buffer );
}

/**
 * Set/Create data failure callback
 */
static void my_data_failure_handler( ids_request_id_t request_id,
                                     ids_result_t result,
                                     const char* info,
                                     void* cb_data ) {
    char buffer[ 12 ] = {0};

    // can't set data
    // Handle the error appropriately.
    // See the header file for the full list of possible result codes that can be returned.
    switch( result ) {
        case IDS_DOES_NOT_EXIST:
            fprintf( stderr, "Can't set data that doesn't exist, doing a create instead\n" );
            snprintf( buffer, sizeof( buffer ), "%d", pressCount + 1 ); // update pressCount
            ids_data_t data;
            data.name = "pressCount";
            data.value = buffer;
            data.length = strlen( data.value ) + 1; // plus null terminator for easier handling when retrieved, but not required

            ids_result_t rc = ids_create_data( profile_provider, IDS_PROFILE_TYPE_APP, IDS_PROFILE_CREATE_DATA_CACHE, &data, my_data_success_handler, my_data_failure_handler, NULL, NULL );
            if( rc != IDS_SUCCESS ) {
                fprintf( stderr, "Failed to create user data.  %s\n", strerror( errno ) );
                setStatusText( "Unable to create profile information" );
            } else {
                fprintf( stderr, "Creating profile info for the first time ...\n" );
                setStatusText( "Creating ..." );
            }
            break;
        case IDS_USER_COULD_NOT_BE_AUTHENTICATED:
            fprintf( stderr, "Failure: %d\n", result );
            setStatusText( "You didn't login to BBID :(" );
            break;
        default:
            fprintf( stderr, "Failure: %d\n", result );
            setStatusText( "Failed - Unable to store profile info" );
            break;
    }
}

/**
 * Trigger request to update the press count and store it in the BlackBerry Profile.  In this sample, this is triggered by pressing the button.
 */
void identity_trigger_set_data() {
    char buffer[ 12 ] = {0};
    snprintf( buffer, sizeof( buffer ), "%d", pressCount + 1 ); // update pressCount
    ids_data_t data;
    data.name = "pressCount";
    data.value = buffer;
    data.length = strlen( data.value ) + 1; // plus null terminator for easier handling when retrieved, but not required

    fprintf( stderr, "Sending request\n" );
    setStatusText( "Setting ..." );

    ids_result_t rc = ids_set_data( profile_provider, IDS_PROFILE_TYPE_APP, IDS_PROFILE_SET_DATA_CACHE, &data, my_data_success_handler, my_data_failure_handler, NULL, NULL );
    if( rc != IDS_SUCCESS ) {
        fprintf( stderr, "Failed to set user data.  %s\n", strerror( errno ) );
        setStatusText( "Unable to set profile information" );
    }
}

/**
 * get_data success callback
 * Update the UI with the press count data
 */
static void my_get_data_success_handler( ids_request_id_t request_id,
                                         const ids_data_t* data,
                                         void* cb_data ) {
    fprintf( stderr, "Successfully got profile data.\n" );
    setStatusText( "Successfully got profile data" );
    pressCount = atoi( data->value );

    // update display with new pressCount
    char buffer[64];
    snprintf( buffer, sizeof( buffer ), "Pressed %d times before", pressCount );
    setCountText( buffer );

}

/**
 * get_data failure callback
 */
static void my_get_data_failure_handler( ids_request_id_t request_id,
                                         ids_result_t result,
                                         const char* info,
                                         void* cb_data ) {
    fprintf( stderr, "Failed to get profile data.  %d\n", result );
    // Handle the error appropriately.
    // See the header file for the full list of possible result codes that can be returned.
    switch( result ) {
        case IDS_DOES_NOT_EXIST:
            // pressCount never previously stored ...
            fprintf( stderr, "No stored profile data to retrieve." );
            setStatusText( "You have never pressed the button before!" );
            pressCount = 0;
            break;
        case IDS_NOT_ALLOWED:
            setStatusText( "Did you allow app access to internet?" );
            break;
        case IDS_ERROR_WHILE_CONTACTING_SERVICE:
            setStatusText( "Can't reach Profile data, do you data connectivity?" );
            break;
        default:
            setStatusText( "Can't get your profile" );
            break;
    }
}

/**
 * get_properties success handler.  Update the UI with user's name,
 * then request the button press count from the BlackBerry Profile storage.
 */
static void my_properties_success_handler( ids_request_id_t request_id,
                                           int property_count,
                                           const ids_property_t* properties,
                                           void* cb_data ) {
    fprintf( stderr, "Successfully retrieved properties.\n" );
    setStatusText( "Success" );

    char buffer[128] = {0};
    int i = 0;
    // Update display with firstname if available, screen name as a backup
    for( ; i < property_count; i++ ) {
        if( ( strcmp( properties[ i ].name, IDS_BBID_PROP_FIRSTNAME ) == 0 )
            && ( strlen( properties[ i ].value ) > 0 ) ) {
            snprintf( buffer, sizeof( buffer ), "Welcome %s", properties[ i ].value );
            setTitleText( buffer );
            break;
        } else if( ( strcmp( properties[ i ].name, IDS_BBID_PROP_SCREENNAME ) == 0 )
                && ( strlen( properties[ i ].value ) > 0 ) ) {
            snprintf( buffer, sizeof( buffer ), "Welcome %s", properties[ i ].value );
            setTitleText( buffer );
        }
    }

    // get the number of times the button has been pressed from the the profile storage
    ids_result_t result = ids_get_data( profile_provider, IDS_PROFILE_TYPE_APP, IDS_PROFILE_GET_DATA_CACHE, "pressCount", my_get_data_success_handler, my_get_data_failure_handler, NULL, NULL );
    if( result != IDS_SUCCESS ) {
        fprintf( stderr, "Failed to get user data.  %s\n", strerror( errno ) );
        setStatusText( "Unable to get profile information" );
    } else {
        setStatusText( "Getting user profile data ..." );
    }
}

/**
 * get_properties failure callback
 */
static void my_properties_failure_handler( ids_request_id_t request_id,
                                           ids_result_t result,
                                           const char* info,
                                           void* cb_data ) {
    fprintf( stderr, "Failed to get user properties.  %d\n", result );
    // Handle the error appropriately.
    // See the header file for the full list of possible result codes that can be returned.
    switch( result ) {
        case IDS_USER_COULD_NOT_BE_AUTHENTICATED:
            setStatusText( "You didn't login to BBID :(" );
            break;
        case IDS_ACCOUNT_LOCALLY_LOCKED_OUT:
            setStatusText( "Identity is locked for a bit" );
            break;
        case IDS_DOES_NOT_EXIST:
        case IDS_NOT_ALLOWED:
            setStatusText( "Did you allow app access to personal info?" );
            break;
        default:
            setStatusText( "Can't get your Identity info" );
            break;
    }
}

/**
 * Trigger request for the user's name so it can be displayed on the screen
 * If successful, the success callback will then trigger request for the currently stored press count
 *
 * In cases where there is no BlackBerry ID user logged into the device, the BlackBerry ID provider will automatically
 * pop up a login dialog on top of the application to have the user log in, after which the application will receive
 * the response callback.  No special integration needs to be performed by the application for non-logged in users with
 * the BlackBerry ID or BlackBerry Profile providers.
 */
void identity_trigger_get_username() {
    const char* propertyList[] = { IDS_BBID_PROP_SCREENNAME, IDS_BBID_PROP_FIRSTNAME };
    ids_result_t result = ids_get_properties( bbid_provider, BBID_PROPERTY_CORE, 2, propertyList, my_properties_success_handler, my_properties_failure_handler, NULL, NULL );
    if( result != IDS_SUCCESS ) {
        fprintf( stderr, "Failed to get user properties.  %s\n", strerror( errno ) );
        setStatusText( "Unable to get IDENTITY information" );
    }
    setStatusText( "Getting user properties ..." );
}
