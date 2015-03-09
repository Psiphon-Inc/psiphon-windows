/*
 * Copyright (c) 2008-2011 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MCTRL_DEFS_H
#define MCTRL_DEFS_H

#include <windows.h>
#include <oaidl.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Common definitions
 *
 * @warning You should not include this header directly. It is included by all
 * public mCtrl headers.
 *
 * This helper header file provides some macro definitions shared by other
 * public mCtrl headers.
 */


/**
 * Helper macro specifying the calling convention of functions exported
 * by @c MCTRL.DLL. You should not use it directly.
 */
#define MCTRL_API                   __stdcall


#ifdef UNICODE
     /** Helper macro for unicode resolution. You should not use it directly. */
     #define MCTRL_NAME_AW(name)    name##W
#else
     #define MCTRL_NAME_AW(name)    name##A
#endif


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_DEFS_H */
