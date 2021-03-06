/**
 * Copyright (c) 2016, Autonomous Networks Research Group. All rights reserved.
 * Developed by:
 * Autonomous Networks Research Group (ANRG)
 * University of Southern California
 * http://anrg.usc.edu/
 *
 * Contributors:
 * Yutong Gu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * - Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimers.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimers in the 
 *     documentation and/or other materials provided with the distribution.
 * - Neither the names of Autonomous Networks Research Group, nor University of 
 *     Southern California, nor the names of its contributors may be used to 
 *     endorse or promote products derived from this Software without specific 
 *     prior written permission.
 * - A citation to the Autonomous Networks Research Group must be included in 
 *     any publications benefiting from the use of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH 
 * THE SOFTWARE.
 */

#ifndef DATA_CONV_H
#define DATA_CONV_H

#include <math.h>
#include <inttypes.h>

typedef struct __attribute__((packed)) {
    float        distance;      
    float      	 angle;
    uint8_t		 node_id;                  
} dist_angle_t;

/**
 * @brief      This function converts the raw TDoA value to a distance
 *
 * @param[in]  tdoa  The TDoA value
 *
 * @return     The distance corresponding to that TDoA value in feet
 */
float get_dist(int tdoa);

/**
 * @brief      This function converts the TDoA of two sensors into an angle estimate
 *
 * @param[in]  a     TDoA of the first sensor
 * @param[in]  b     TDoA of the second sensor
 *
 * @return     The angle at which the two sensors are facing the transmitter in degrees
 */
float get_angle(float a, float b);

/**
 * @brief      Calculates the middle distance between the sensors and transmitter.
 *
 * @param[in]  a     TDoA of the first sensor
 * @param[in]  b     TDoA of the second sensor
 *
 * @return     The the middle distance between the sensors and transmitter
 */
float get_mid_dist(float a, float b);

#endif