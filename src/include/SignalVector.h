/*
    bbs100 2.1 WJ104
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
	SignalVector.h	WJ99
*/

#ifndef SIGNALVECTOR_H_WJ99
#define SIGNALVECTOR_H_WJ99 1

#include "List.h"

#define add_SignalVector(x,y)			add_List((x), (y))
#define concat_SignalVector(x,y)		concat_List((x), (y))
#define remove_SignalVector(x,y)		remove_List((x), (y))
#define rewind_SignalVector(x,y)		(SignalVector *)rewind_List((x), (y))
#define unwind_SignalVector(x,y)		(SignalVector *)unwind_List((x), (y))
#define listdestroy_SignalVector(x)		listdestroy_SignalVector((x), destroy_SignalVector)

typedef struct SignalVector_tag SignalVector;

struct SignalVector_tag {
	List(SignalVector);

	void (*handler)(int);
};

SignalVector *new_SignalVector(void (*)(int));
void destroy_SignalVector(SignalVector *);

#endif	/* SIGNALVECTOR_H_WJ99 */

/* EOB */
