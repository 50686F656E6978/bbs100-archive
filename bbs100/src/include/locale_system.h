/*
    bbs100 3.0 WJ105
    Copyright (C) 2005  Walter de Jong <walter@heiho.net>

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
	locale_system.h	WJ105
*/

#ifndef LOCALE_SYSTEM_H_WJ105
#define LOCALE_SYSTEM_H_WJ105	1

#include "locales.h"

extern Locale system_locale;
extern Locale *lc_system;

char *lc_print_date(Locale *, struct tm *, int, char *, int);
char *lc_print_total_time(Locale *, unsigned long, char *, int);
char *lc_print_number(unsigned long, int, char *, int);
char *lc_print_number_commas(Locale *, unsigned long, char *, int);
char *lc_print_number_dots(Locale *, unsigned long, char *, int);
char *lc_print_numberth(Locale *, unsigned long, char *, int);
char *lc_possession(Locale *, char *, char *, char *, int);

#endif	/* LOCALE_SYSTEM_H_WJ105 */

/* EOB */