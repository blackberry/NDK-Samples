/*
 * main.c
 *
 *  Created on: Jun 17, 2013
 *      Author: svelikevitch
 */

//#include "smartcard/sc_data_types.h"
//#include "smartcard/sc_key_store_data.h"
//#include "smartcard/sc_reader_state.h"
//#include "smartcard/sc_certificate.h"
//#include "smartcard/sc_crypto_token.h"
//#include "smartcard/sc_crypto_system.h"
//#include "smartcard/sc_smart_card_id.h"

#include <stdlib.h>
#include "smartcard/sc_smart_card.h"

#define CHECKRC( rc ) { if( rc != 0 ) goto end; }


int main()
{
	sc_reader_name_t readername = "MediaCardSmartCardReader";
	sc_response_code_t rc = SC_SCARD_S_SUCCESS;

    sc_context_t* context = 0;

    sc_card_name_t *cardnames = 0;
    size_t cardsize = 0;
    sc_smart_card_id_t* id = 0;
    long long cardid = 0;

    // Creates a new context used to perform individual smart card operations.
    rc = sc_context_create( &context );
    CHECKRC( rc );

    // Allocates a handle for the specified resource. Resources are identified by
    // type and name. While all resources have a specific type, not all resources
    // have an associated name. Generally, all reader and card resources require a
    // name to be specified.
    // Note that only a single instance of each type of handle can be stored within
    // a context. Attempting to associate multiple instances of a single type of
    // handle will result in an error to be returned.
    rc = sc_allocate_handle( context, SC_HANDLE_READER_SLOT_LOGICAL, readername );
    CHECKRC( rc );

    // Retrieves the number of available card types.
    rc = sc_get_card_types_for_reader( context, 0, &cardsize );
    CHECKRC( rc );

    // Retrieves list of card types matching the ATR of the card in the reader
    // associated with the provided context. This function is useful in
    // determining which card types can be used to manipulate the card in the
    // reader.
    cardnames = malloc( sizeof( sc_card_name_t ) * cardsize );
    rc = sc_get_card_types_for_reader( context, cardnames, &cardsize );
    CHECKRC( rc );

    // Allocates a handle for the specified resource. Resources are identified by
    // type and name. While all resources have a specific type, not all resources
    // have an associated name. Generally, all reader and card resources require a
    // name to be specified.
    // Note that only a single instance of each type of handle can be stored within
    // a context. Attempting to associate multiple instances of a single type of
    // handle will result in an error to be returned.
    rc = sc_allocate_handle( context, SC_HANDLE_CARD_HOLDER_VERIFIER, cardnames[ 0 ] );
    CHECKRC( rc );

    // Opens a session to the smart card located in a given reader. If the reader
    // is unknown to the smart card subsystem, then an error is returned. Other
    // connection options will be set by the driver to optimize communication
    // with the card.
    // Before opening a session to a smart card, a card handle must first have been
    // allocated. Otherwise, an error is returned.
    rc = sc_attach_by_reader( context, readername, SC_SCARD_SHARE_EXCLUSIVE );
    CHECKRC( rc );

    // Creates sc_smart_card_id_t instance.
    rc = sc_smart_card_id_create( &id );
    CHECKRC( rc );

    // Retrieves an identifier uniquely identifying the currently connected smart card.
    rc = sc_get_smart_card_id( context, id );
    CHECKRC( rc );

    // Retrieves the card numeric identifier.
    rc = sc_smart_card_id_get_card_id( id, &cardid );
    CHECKRC( rc );

    // Closes any open smart card session.
    rc = sc_detach( context, SC_SCARD_RESET_CARD );
    CHECKRC( rc );


end:
	// Destroys sc_smart_card_id_t instance.
    sc_smart_card_id_destroy( id );

    // Deallocates specified handle.
    sc_free_handle( context, SC_HANDLE_READER_SLOT_LOGICAL );

    // Deallocates specified handle.
    sc_free_handle( context, SC_HANDLE_CARD_HOLDER_VERIFIER );

    // Destroys the specified context.
    sc_context_destroy( context );

    // Frees memory.
    free( cardnames );

    return rc;
}

