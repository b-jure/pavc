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

#ifndef PSVC_UTIL_H
#define PSVC_UTIL_H

#include <stddef.h>

void panic(const char* efmt, ...);
int stoui(const char* s, unsigned int* i);

#define unused(x) (void)(x)

#endif
