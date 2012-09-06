/**
 *
 * The Touchpoint Header
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


#ifndef TOUCHPOINT_H_
#define TOUCHPOINT_H_

#include <list>

class Touchpoint {
public:
	Touchpoint(int,int,int);
	Touchpoint(const Touchpoint& p);
	virtual ~Touchpoint();

	float r;
	float g;
	float b;
	int x;
	int y;
	int startRotation;
	unsigned int id;

	bool visible;

	Touchpoint& operator=(const Touchpoint &source);
	void updatePosition(int,int);
	int getX();
	int getY();
	int getStartRotation();
	void setInvisible(void);
	void setColor(float,float,float);
};


#endif /* TOUCHPOINT_H_ */
