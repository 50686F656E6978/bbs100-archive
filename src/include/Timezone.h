/*
    bbs100 1.2.2 WJ103
    Copyright (C) 2003  Walter de Jong <walter@heiho.net>

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
	Timezone.h	WJ103
*/

#ifndef TIMEZONE_H_WJ103
#define TIMEZONE_H_WJ103	1

#include "Hash.h"

#include <time.h>


typedef struct {
	time_t when;			/* when DST transition occurs */
	int type_idx;			/* 'local time type'; index to TimeType structs */
} DST_Transition;

typedef struct {
	int gmtoff;				/* offset to GMT (e.g. 3600 for GMT+0100) */
	int isdst;				/* is Daylight Savings in effect? (summer time) */
	int tzname_idx;			/* pointer to tznames; name of the timezone */
} TimeType;

typedef struct {
	time_t when;
	int num_secs;
} LeapSecond;

typedef struct {
	int refcount;
	int curr_idx, next_idx;

	int num_trans, num_types;

	DST_Transition *transitions;
	TimeType *types;

	char *tznames;
} Timezone;

extern Hash *tz_hash;

int init_Timezone(void);

Timezone *load_Timezone(char *);
void unload_Timezone(char *);

#endif	/* TIMEZONE_H_WJ103 */

/* EOB */