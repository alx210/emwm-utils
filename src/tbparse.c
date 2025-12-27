/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
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

/*
 * Toolbox configuration file parser
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include "tbparse.h"

static char* get_line(void);
static char* skip_blanks(char *p);
static void parse_line(char *line, struct tb_entry *e);
static void set_parse_error(int line, const char *text);
static struct tb_entry* add_entry(const struct tb_entry *ent);
static int parse_buffer(void);

#define MAX_PARSE_ERROR	256
static char parse_error[MAX_PARSE_ERROR];

static char *buffer = NULL;
static char *buf_ptr = NULL;
struct tb_entry *entries = NULL;

/* Get next line from global buffer */
static char* get_line(void)
{
	char *p, *cur=buf_ptr;
	
	if(*buf_ptr == '\0') return NULL;
	
	p=strchr(buf_ptr,'\n');
	
	if(p){
		buf_ptr = p+1;
		*p = '\0';
	}else{
		while(*buf_ptr != '\0') buf_ptr++;
	}
	return cur;
}

static char* skip_blanks(char *p)
{
	while(*p == '\t' || *p == ' ') p++;
	return p;
}

/* Parses a line into the given entry structure */
static void parse_line(char *line, struct tb_entry *e)
{
	char *p=line;
	
	memset(e,0,sizeof(struct tb_entry));
	
	if(!strcmp(line,"SEPARATOR")){
		e->type=TBE_SEPARATOR;
		return;	
	}
	
	e->title=line;
	
	while(*p != '\0'){
		if(*p == '\\' && (p[1] == '\\' || p[1] == '&' || p[1] == ':')){
			memmove(p,p+1,strlen(p+1)+1);
		}else if(*p == '&'){
			e->mnemonic=p[1];
			memmove(p,p+1,strlen(p+1)+1);
		}else if(*p == ':'){
			e->command=skip_blanks(p+1);
			*p='\0';
			break;
		}
		p++;
	}
	if(e->command)
		e->type=TBE_COMMAND;
	else
		e->type=TBE_CASCADE;
}

static void set_parse_error(int line, const char *text)
{
	snprintf(parse_error,MAX_PARSE_ERROR,"Line %d: %s",line,text);
}

/* Duplicates the given entry and adds it to the global list */
static struct tb_entry* add_entry(const struct tb_entry *ent)
{
	struct tb_entry *new;

	new = malloc(sizeof(struct tb_entry));
	if(!new) return NULL;
	memcpy(new, ent, sizeof(struct tb_entry));
	
	if(!entries){
		entries = new;
		new->next = NULL;
	} else {
		struct tb_entry *last = entries;
		while(last->next) last = last->next;
		
		last->next = new;
	}
	return new;
}

/* Parses the global buffer */
static int parse_buffer(void)
{
	char *line;
	struct tb_entry tmp;
	struct tb_entry *prev=NULL;
	int nlevel=0;
	int iline=0;
	
	while((line = get_line())){
		iline++;
		line=skip_blanks(line);
		
		if(*line == '\0' || *line == '#'){
			continue;
		}else if(*line == '{'){
			if(!prev || prev->type != TBE_CASCADE){
				set_parse_error(iline,
					"Delimiter \'{\' must follow a cascade entry");
				return -1;
			}
			nlevel++;
			continue;
		}else if(*line == '}'){
			if(!nlevel || prev->type != TBE_COMMAND){
				set_parse_error(iline,"Delimiter \'}\' out of scope");
				return -1;
			}
			nlevel--;
			continue;
		}else if(prev && prev->type == TBE_CASCADE && prev->level == nlevel){
			set_parse_error(iline,"Cascade entry must have a menu scope");
			return -1;
		}
		
		parse_line(line,&tmp);
		if(tmp.type == TBE_COMMAND) {
			if(nlevel < 1){
				set_parse_error(iline,
					"Command entries must reside within a menu scope");
				return -1;
			}
			if(!strlen(tmp.command)) {
				set_parse_error(iline,
					"Command string expected after ':' ");
				return -1;
			}
		}
		
		tmp.level=nlevel;
		if((prev=add_entry(&tmp))==NULL) return ENOMEM;
	}
	return 0;
}


/*
 * Parses a toolbox menu file.
 * Returns zero on success or errno otherwise.
 * If EINVAL is returned, the file contains syntax errors, and
 * tb_parser_error_string() may be used to obtain detailed information.
 */
int tb_parse_config(const char *filename, struct tb_entry **ent_root)
{
	FILE *file;
	struct stat st;
	int err;
	char *old_buf = buffer;
	struct tb_entry *old_ent = entries;
	
	entries = NULL;
	parse_error[0]='\0';
	
	if(stat(filename, &st) < 0) return errno;
	if(st.st_size == 0) return EIO;
	
	if(!(buffer = malloc(st.st_size + 1))) {
		buffer = old_buf;
		return errno;
	}

	buf_ptr = buffer;
	
	file = fopen(filename, "r");
	if(!file){
		err = errno;
		free(buffer);
		buffer = old_buf;
		return err;
	}

	if(fread(buffer, 1, st.st_size, file) < st.st_size){
		err = errno;
		free(buffer);
		fclose(file);
		buffer = old_buf;
		return err;
	}
	fclose(file);
		
	buffer[st.st_size] = '\0';

	if((err = parse_buffer())){
		free(buffer);
		buffer = old_buf;
		return err;
	}

	if(old_ent){
		struct tb_entry *tmp, *cur = old_ent;
		while(cur){
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}

	*ent_root = entries;

	return 0;
}

char* tb_parser_error_string(void)
{
	return (parse_error[0]=='\0')?NULL:parse_error;
}
