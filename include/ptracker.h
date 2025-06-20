// MIT License
//
// Copyright (c) 2025 Catmengi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.




#pragma once
#include <sys/types.h>

typedef struct _prec* prec_t;

struct prec_callbacks{
    void (*increment)(prec_t prec, void* udata);
    void (*decrement)(prec_t prec, void* udata);
    void (*zero)(prec_t prec);
};

prec_t prec_get(void* ptr);
prec_t prec_new(void* ptr, struct prec_callbacks cbs);
void prec_delete(prec_t prec);
void prec_increment(prec_t prec, void* udata);
void prec_decrement(prec_t prec, void* udata);
int prec_refcount(prec_t prec);
void* prec_context_get(prec_t prec);
void prec_context_set(prec_t prec, void* context);
void* prec_ptr(prec_t prec);
