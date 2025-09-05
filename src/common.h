/*
 * Copyright (C) 2023-2025 alx@fastestcode.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef COMMON_H
#define COMMON_H

#define VER_MAJOR 1
#define VER_MINOR 3
#define VER_UPLVL 1

/* Reliable signal handling (using POSIX sigaction) */
typedef void (*sigfunc_t)(int);
sigfunc_t rsignal(int sig, sigfunc_t);

/*
 * Expands sh style environment variables found in 'in' and returns the
 * expanded string in 'out', which is allocated from the heap and must be
 * freed by the caller. Returns zero on success, errno otherwise.
 */
int expand_env_vars(const char *in, char **out);

char* get_login(void);

void print_version(const char*);

#endif /* COMMON_H */
