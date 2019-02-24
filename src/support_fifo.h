//******************************************************************************************************
//
// file:      support_fifo.h
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
#ifndef SUPPORT_FIFO_H
#define SUPPORT_FIFO_H

#include <Arduino.h>

#define FIFO_SIZE 16

class FIFO {

public:
  FIFO();
  ~FIFO();
  void empty();
  void push(uint8_t data);
  uint8_t pop();
  uint8_t size();

private:
  uint8_t head;
  uint8_t tail;
  uint8_t numElements;
  uint8_t buffer[FIFO_SIZE];
  
};

#endif
