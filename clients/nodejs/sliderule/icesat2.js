// Copyright (c) 2021, University of Washington
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the University of Washington nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
// “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import { EventEmitter } from 'events';
import {core} from '../sliderule/index.js';
//------------------------------------
// File Data
//------------------------------------

//
// ICESat-2 Parameters
//
const CNF_POSSIBLE_TEP = -2;
const CNF_NOT_CONSIDERED = -1;
const CNF_BACKGROUND = 0;
const CNF_WITHIN_10M = 1;
const CNF_SURFACE_LOW = 2;
const CNF_SURFACE_MEDIUM = 3;
const CNF_SURFACE_HIGH = 4;
const SRT_LAND = 0;
const SRT_OCEAN = 1;
const SRT_SEA_ICE = 2;
const SRT_LAND_ICE = 3;
const SRT_INLAND_WATER = 4;
const MAX_COORDS_IN_POLYGON = 16384;
const GT1L = 10;
const GT1R = 20;
const GT2L = 30;
const GT2R = 40;
const GT3L = 50;
const GT3R = 60;
const STRONG_SPOTS = [1, 3, 5];
const WEAK_SPOTS = [2, 4, 6];
const LEFT_PAIR = 0;
const RIGHT_PAIR = 1;
const SC_BACKWARD = 0;
const SC_FORWARD = 1;
const ATL08_WATER = 0;
const ATL08_LAND = 1;
const ATL08_SNOW = 2;
const ATL08_ICE = 3;

//
// PhoREAL Percentiles
//
const P = { '5':   0, '10':  1, '15':  2, '20':  3, '25':  4, '30':  5, '35':  6, '40':  7, '45':  8, '50': 9,
      '55': 10, '60': 11, '65': 12, '70': 13, '75': 14, '80': 15, '85': 16, '90': 17, '95': 18 };

//------------------------------------
// Exported Functions
//------------------------------------

//
// ATL06P
//
export function atl06p(parm, resources, callbacks = null){
    console.log("atl06p: ", parm, resources);
    if (!('asset' in parm)) {
        parm['asset'] = 'icesat2';
    }
    let rqst = {
        "resources": resources,
        "parms": parm
    };
    if (callbacks != null) {
        return core.source('atl06p', rqst, true, callbacks);
    } else {
        let event = new EventEmitter();
        let total_recs = null;
        let recs = [];
        callbacks = {
            atl06rec: (result) => {
                recs.push(result["elevation"]);
                if (total_recs != null && recs.length == total_recs) {
                    event.emit('complete');
                }
            },
        };
        return new Promise(resolve => {
            core.source('atl06p', rqst, true, callbacks).then(
                result => {
                    event.once('complete', () => {
                        resolve(recs.flat(1));
                    });
                    total_recs = result["atl06rec"] ?? 0;
                    if (recs.length == total_recs) {
                        event.emit('complete');
                    }
                }
            );
        });
    }
};

// Export any other constants or functions if necessary
export {
    CNF_POSSIBLE_TEP,
    CNF_NOT_CONSIDERED,
    CNF_BACKGROUND,
    CNF_WITHIN_10M,
    CNF_SURFACE_LOW,
    CNF_SURFACE_MEDIUM,
    CNF_SURFACE_HIGH,
    SRT_LAND,
    SRT_OCEAN,
    SRT_SEA_ICE,
    SRT_LAND_ICE,
    SRT_INLAND_WATER,
    MAX_COORDS_IN_POLYGON,
    GT1L,
    GT1R,
    GT2L,
    GT2R,
    GT3L,
    GT3R,
    STRONG_SPOTS,
    WEAK_SPOTS,
    LEFT_PAIR,
    RIGHT_PAIR,
    SC_BACKWARD,
    SC_FORWARD,
    ATL08_WATER,
    ATL08_LAND,
    ATL08_SNOW,
    ATL08_ICE
  };
  