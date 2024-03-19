/* Copyright (C) 2024 Jure Bagić
 *
 * This file is part of pavc.
 * pavc is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * pavc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with pavc.
 * If not, see <https://www.gnu.org/licenses/>. */


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void
panic(const char* efmt, ...) 
{
        va_list ap;

        if(efmt) {
                va_start(ap, efmt);
                vfprintf(stderr, efmt, ap);
                va_end(ap);
                fflush(stderr);
        }
        exit(1);
}


int
stoui(const char* s, unsigned int* i)
{
        int c;

        *i = 0;
        while((c = *s++))
                if(c > '9' || c < '0')
                        return -1;
                else
                        *i = *i * 10 + (c - '0');
        return 0;
}
