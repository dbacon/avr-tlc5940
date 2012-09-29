//
//   Copyright 2012 Dave Bacon
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#ifndef __delay_h__
#define __delay_h__

#include <inttypes.h>

#ifdef  __cplusplus
extern "C" {
#endif

void delay3(uint16_t p); // __attribute__ ((noinline));
void delay_100us(uint16_t p); // __attribute__ ((noinline));
void delay_ms(uint16_t p); // __attribute__ ((noinline));

#ifdef __cplusplus
}
#endif

#endif // __delay_h__
