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

#include "delay.h"

// executes 2 instructions for each p value
void delay2(uint16_t p) {
	while (--p) asm("");
}

void delay_100us(uint16_t p) {
	while (--p) delay2(500);
}

void delay_ms(uint16_t p) {
	while (--p) delay_100us(10);
}
