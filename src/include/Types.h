/*
    bbs100 1.2.0 WJ102
    Copyright (C) 2002  Walter de Jong <walter@heiho.net>

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
	Types.h	WJ100
*/

#ifndef TYPES_H_WJ100
#define TYPES_H_WJ100		1

#define TYPE_CHAR			0
#define TYPE_POINTER		1
#define TYPE_STRINGLIST		2
#define TYPE_PLIST			3
#define TYPE_CALLSTACK		4
#define TYPE_SYMBOLTABLE	5
#define TYPE_TIMER			6
#define TYPE_SIGNALVECTOR	7
#define TYPE_USER			8
#define TYPE_ONLINEUSER		9
#define TYPE_ROOM			10
#define TYPE_JOINED			11
#define TYPE_MESSAGE		12
#define TYPE_MSGINDEX		13
#define TYPE_FEELING		14
#define TYPE_BUFFEREDMSG	15
#define TYPE_FILE			16
#define TYPE_WRAPPER		17
#define TYPE_CACHEDFILE		18
#define TYPE_HOSTMAP		19
#define TYPE_ATOMICFILE		20
#define TYPE_SU_PASSWD		21
#define TYPE_ZONEINFO		22
#define NUM_TYPES			23

typedef struct Typedef_tag Typedef;

struct Typedef_tag {
	char *type;
	int size;
};

extern Typedef Types_table[NUM_TYPES+1];

#endif	/* TYPES_H_WJ100 */

/* EOB */
