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
	ipv4_aton.c	WJ104
*/

#include "config.h"
#include "ipv4_aton.h"

#include <stdio.h>
#include <stdlib.h>

/*  $Id: inet_aton.c,v 1.5 2001/12/12 09:06:00 rra Exp $
**
**  Replacement for a missing inet_aton.
**
**  Written by Russ Allbery <rra@bogus.example.com>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard library routine
**  inet_aton for those platforms that don't have it. inet_aton is
**  thread-safe
*/

int ipv4_aton(char *s, struct in_addr *addr) {
unsigned long octet[4], address;
char *p;
int base, i;
int part = 0;

	if (s == NULL)
		return 0;

/* Step through each period-separated part of the address. If we see
	more than four parts, the address is invalid. */

	for (p = s; *p != 0; part++) {
		if (part > 3)
			return 0;

/*
	Determine the base of the section we're looking at. Numbers are
	represented the same as in C; octal starts with 0, hex starts
	with 0x, and anything else is decimal
*/
		if (*p == '0') {
			p++;
			if (*p == 'x') {
				p++;
				base = 16;
			} else
				base = 8;
		} else
			base = 10;

/*
	Make sure there's actually a number. (A section of just "0"
	would set base to 8 and leave us pointing at a period; allow
	that.)
*/
		if (*p == '.' && base != 8)
			return 0;

		octet[part] = 0;

/*
	Now, parse this segment of the address.  For each digit, multiply
	the result so far by the base and then add the value of the
	digit.  Be careful of arithmetic overflow in cases where an
	unsigned long is 32 bits; we need to detect it *before* we
	multiply by the base since otherwise we could overflow and wrap
	and then not detect the error
*/
		for (; *p != 0 && *p != '.'; p++) {
			if (octet[part] > 0xffffffffUL / base)
				return 0;

/*
	Use a switch statement to parse each digit rather than
	assuming ASCII. Probably pointless portability ...
*/
			switch (*p) {
				case '0':			i = 0;	break;
				case '1':			i = 1;	break;
				case '2':			i = 2;	break;
				case '3':			i = 3;	break;
				case '4':			i = 4;	break;
				case '5':			i = 5;	break;
				case '6':			i = 6;	break;
				case '7':			i = 7;	break;
				case '8':			i = 8;	break;
				case '9':			i = 9;	break;
				case 'A': case 'a':	i = 10;	break;
				case 'B': case 'b':	i = 11;	break;
				case 'C': case 'c':	i = 12;	break;
				case 'D': case 'd':	i = 13;	break;
				case 'E': case 'e':	i = 14;	break;
				case 'F': case 'f':	i = 15;	break;
				default:
					return 0;
			}
			if (i >= base)
				return 0;

			octet[part] = (octet[part] * base) + i;
		}

/*
	Advance over periods; the top of the loop will increment the
	count of parts we've seen.  We need a check here to detect an
	illegal trailing period
*/
		if (*p == '.') {
			p++;
			if (*p == 0)
				return 0;
		}
	}
	if (part == 0)
		return 0;

/*
	IPv4 allows three types of address specification:

	a.b
	a.b.c
	a.b.c.d

	If there are fewer than four segments, the final segment accounts for
	all of the remaining portion of the address. For example, in the a.b
	form, b is the final 24 bits of the address. We also allow a simple
	number, which is interpreted as the 32-bit number corresponding to
	the full IPv4 address

	The first for loop below ensures that any initial segments represent
	only 8 bits of the address and builds the upper portion of the IPv4
	address. Then, the remaining segment is checked to make sure it's no
	bigger than the remaining space in the address and then is added into
	the result
*/
	address = 0;
	for (i = 0; i < part - 1; i++) {
		if (octet[i] > 0xff)
			return 0;

		address |= octet[i] << (8 * (3 - i));
	}
	if (octet[i] > (0xffffffffUL >> (i * 8)))
		return 0;

	address |= octet[i];
	if (addr != NULL)
		addr->s_addr = htonl(address);

	return 1;
}

/* EOB */
