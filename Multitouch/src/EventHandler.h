/**
 *
 * The Eventhandler Header
 *
 *
 * Copyright (c) 2012 mappau OG
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
 *
 * feel free to contact us:
 * blackberry@mappau.com
 * http://www.mappau.com
 *
 */

#ifndef EVENTHANDLER_H_
#define EVENTHANDLER_H_

#include <bps/bps.h>
#include <bps/event.h>
#include "Touchpoint.h"


extern int closeApplication;
extern int navigator_domain;
extern std::list<Touchpoint> points;
extern int oriention_side_up;

void handleEvent(void);
void init(void);
void handleScreenEvent(bps_event_t *event);
void handleNavigatorEvent(bps_event_t *event);
void handle_dialog_response(bps_event_t *event);
void show_alert();
int resize(bps_event_t *event);



#endif /* EVENTHANDLER_H_ */
