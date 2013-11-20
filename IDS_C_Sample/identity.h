/*
 * identity.h
 *
 * Integration with the IDS library for interacting with the User Identity on the device
 */

#ifndef IDENTITY_H_
#define IDENTITY_H_

#include <bps/bps.h>
#include <ids.h>
#include <ids_blackberry_id.h>
#include <ids_blackberry_profile.h>

int identity_initialize( const char* windowGroupId );
void identity_shutdown();

void identity_trigger_get_username();
void identity_trigger_set_data();

#endif /* IDENTITY_H_ */
