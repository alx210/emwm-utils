/*
 * Copyright (C) 2023 alx@fastestcode.org
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


#include <stdlib.h>
#include <signal.h>
#include "common.h"

/* Reliable signal handling (using POSIX sigaction) */
sigfunc_t rsignal(int sig, sigfunc_t handler)
{
	struct sigaction set, ret;
	
	set.sa_handler = handler;
	sigemptyset(&set.sa_mask);
	set.sa_flags = SA_NOCLDSTOP;
	
	if(sig == SIGALRM) {
		#ifdef SA_INTERRUPT
		set.sa_flags |= SA_INTERRUPT;
		#endif
	} else {
		set.sa_flags |= SA_RESTART;
	}
	if(sigaction(sig, &set, &ret) < 0)
		return SIG_ERR;
	
	return ret.sa_handler;
}
