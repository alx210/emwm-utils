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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
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

/*
 * Expands sh style environment variables found in 'in' and returns the
 * expanded string in 'out', which is allocated from the heap and must be
 * freed by the caller. Returns zero on success, errno otherwise.
 */
int expand_env_vars(const char *in, char **out)
{
	char *src;
	char *buf;
	char **parts;
	size_t nparts = 0;
	char *s, *p;
	int res = 0;
	
	src = strdup(in);
	if(!src) return ENOMEM;

	s = p = src;
	
	/* count parts and allocate temporary storage */
	while(*p) {
		if((*p == '$') && (p[1] != '$')) nparts++;
		p++;
	}
	
	if(!nparts) {
		*out = src;
		return 0;
	}
	
	parts = calloc((nparts + 1) * 2, sizeof(char*));
	if(!parts) {
		free(src);
		return ENOMEM;
	}
	
	/* reset for parsing */
	nparts = 0;
	s = p = src;
	
	while(*p) {
		if(*p == '$') {
			char vc = *p;
			
			/* double special char stands for literal */
			if(p[1] == vc) {
				memmove(p, p + 1, strlen(p));
				p++;
				continue;
			}
			
			/* starting point of the next part */
			parts[nparts++] = s;
			
			/* explicit scope ${...} */
			if(p[1] == '{') {
				p[0] = '\0';
				p += 2;
				s = p;
				
				while(*p && *p != '}') p++;

				if(*p == '\0') {
					res = EINVAL;
					break;
				}
				
				if(!(p - s)) break;
				
				buf = malloc((p - s) + 1);
				if(!buf) {
					res = errno;
					break;
				}
				memcpy(buf, s, p - s);
				buf[p - s] = '\0';
				
				s--;
				memmove(s, p + 1, strlen(p));
				
				p = s;
			} else { 
				/* implicit scope $...
				 * (eventually terminated by a space, dot, quotes or slash) */
				p[0] = '\0';
				p++;
				s = p;
				
				while(*p && *p != ' ' && *p != '\"' && *p != '\'' &&
					*p != '\t' && *p != '.'&& *p != '\\' && *p != '/') p++;
				
				if(!(p - s)) {
					res = EINVAL;
					break;
				}
				
				buf = malloc((p - s) + 1);
				if(!buf) {
					res = errno;
					break;
				}
				memcpy(buf, s, p - s);
				buf[p - s] = '\0';

				memmove(s , p, strlen(p) + 1);
				
				p = s;
			}

			parts[nparts] = getenv(buf);
			if(parts[nparts] == NULL)
				fprintf(stderr, "Undefined environment variable %s\n", buf);

			nparts++;
			free(buf);
		}
		p++;
	}
	
	if(!res) {
		size_t i, len = 1;
		
		/* add trailing part (if string didn't end with a variable) */
		if(p != s) parts[nparts++] = s;
		p = src;

		for(i = 0; i < nparts; i++) {
			if(parts[i] && *parts[i]) len += strlen(parts[i]);
		}
		
		/* compose parts into a single string */
		buf = malloc(len);
		if(buf) {
			buf[0] = '\0';

			for(i = 0; i < nparts; i++) {
				if(parts[i] && *parts[i]) strcat(buf, parts[i]);
			}
			*out = buf;
		} else {
			res = ENOMEM;
		}
	}
	
	free(parts);
	free(src);
	return res;
}

char* get_login(void)
{
	static char *login = NULL;
	
	if(!login) {
		struct passwd *pwd;
		pwd = getpwuid(getuid());
		if(pwd) login = strdup(pwd->pw_name);
	}
	return login;
}

void print_version(const char *name)
{
	printf("%s %d.%d.%d\n", name, VER_MAJOR, VER_MINOR, VER_UPLVL);
}
