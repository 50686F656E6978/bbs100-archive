/*
    bbs100 2.0 WJ104
    Copyright (C) 2004  Walter de Jong <walter@heiho.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
	StringList.c	WJ98
*/

#include "config.h"
#include "StringList.h"
#include "cstring.h"
#include "Memory.h"
#include "AtomicFile.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

StringList *new_StringList(char *s) {
StringList *sl;

	if (s == NULL || (sl = (StringList *)Malloc(sizeof(StringList), TYPE_STRINGLIST)) == NULL)
		return NULL;

	if ((sl->str = cstrdup(s)) == NULL) {
		Free(sl);
		return NULL;
	}
	return sl;
}

void destroy_StringList(StringList *sl) {
	if (sl == NULL)
		return;

	Free(sl->str);
	Free(sl);
}


StringList *in_StringList(StringList *sl, char *p) {
	if (p == NULL || !*p)
		return NULL;

	while(sl != NULL) {
		if (sl->str != NULL && !strcmp(sl->str, p))
			return sl;
		sl = sl->next;
	}
	return sl;
}

/* converts stringlist to string; returned string must be Free()d */
char *str_StringList(StringList *root) {
char *str;
int i = 2;
StringList *sl;

	for(sl = root; sl != NULL; sl = sl->next)
		if (sl->str != NULL)
			i += strlen(sl->str);

	if ((str = (char *)Malloc(i, TYPE_CHAR)) == NULL)
		return NULL;

	*str = 0;
	for(sl = root; sl != NULL; sl = sl->next) {
		if (sl->str != NULL) {
			strcat(str, sl->str);
			strcat(str, "\n");
		}
	}
	return str;
}

StringList *load_StringList(char *file) {
StringList *sl = NULL;
AtomicFile *f;
char buf[1024];

	if ((f = openfile(file, "r")) == NULL)
		return NULL;

	while(fgets(buf, 1024, f->f) != NULL) {
		chop(buf);
		add_StringList(&sl, new_StringList(buf));
	}
	closefile(f);
	return sl;
}

int save_StringList(StringList *sl, char *file) {
AtomicFile *f;

	if ((f = openfile(file, "w")) == NULL)
		return -1;

	while(sl != NULL) {
		fprintf(f->f, "%s\n", sl->str);
		sl = sl->next;
	}
	closefile(f);
	return 0;
}

StringList *copy_StringList(StringList *sl) {
StringList *root = NULL, *cp = NULL;

	if (sl == NULL)
		return NULL;

	root = cp = new_StringList(sl->str);
	sl = sl->next;

	while(sl != NULL) {
		cp = add_StringList(&cp, new_StringList(sl->str));
		sl = sl->next;
	}
	return root;
}


StringList *vadd_String(StringList **slp, char *fmt, va_list ap) {
StringList *sl;
char buf[PRINT_BUF];

	if (slp == NULL)
		return NULL;

	vsprintf(buf, fmt, ap);
	va_end(ap);
	if ((sl = new_StringList(buf)) == NULL)
		return *slp;

	return add_StringList(slp, sl);
}

StringList *add_String(StringList **slp, char *fmt, ...) {
va_list ap;

	va_start(ap, fmt);
	return vadd_String(slp, fmt, ap);
}

/* EOB */
