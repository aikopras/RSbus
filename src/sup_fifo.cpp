//******************************************************************************************************
//
// file:      sup_fifo.cpp
// author:    Deisterholf / Aiko Pras
// source:    https://github.com/deisterhold/Arduino-FIFO
// history:   2019-01-30 V0.1 ap rewritten, to customize for the RS-bus purpose
//
// purpose:   FIFO functions to store RS-bus data
//
/*
 * FIFO Buffer
 * Implementation uses arrays to conserve memory
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Daniel Eisterhold
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//******************************************************************************************************
#include <Arduino.h>
#include "sup_fifo.h"

FIFO::FIFO() {                     // This is the constructor
  head = 0;
  tail = 0;
  numElements = 0;
}


FIFO::~FIFO() {}                   // This is the destructor


uint8_t FIFO::size() {return numElements;}


void FIFO::push(uint8_t data) {
  if(numElements == FIFO_SIZE) {return;}
  else {
    numElements++;                 // Increment size   
    if(numElements > 1) {          // Only move the tail if there is more than one element
      tail++;                      // Increment tail location     
      tail %= FIFO_SIZE;           // Make sure tail is within the bounds of the array
    }
    buffer[tail] = data;           // Store data into array
  }
}


uint8_t FIFO::pop() {
  if(numElements == 0) {return 0;}
  else {
    numElements--;                 // Decrement size   
    uint8_t data = buffer[head];   // Store the head of the queue in a temporary buffer
    if(numElements >= 1) {
      head++;                      // Move head up one position     
      head %= FIFO_SIZE;           // Make sure head is within the bounds of the array
    }
    return data;
  }
}


void FIFO::empty() {
  head = 0;
  tail = 0;
  numElements = 0;
}
