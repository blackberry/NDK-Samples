/*
 * Copyright (c) 2013 BlackBerry Limited.
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
