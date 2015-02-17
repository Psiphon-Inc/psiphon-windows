/*
 * Copyright (c) 2013 Martin Mitas
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

#ifndef MCTRL_COMMON_H
#define MCTRL_COMMON_H

#include <mCtrl/_defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Common constants and types
 *
 * @warning You should not include this header directly. It is included by all
 * public mCtrl headers.
 *
 * Many mCtrl controls implement features outlined after @c <commctrl.h>,
 * for sake of consistency and for sake of easiness of mCtrl adoption by
 * developers with Win32API experience.
 *
 * However we don't want to include that header in public mCtrl headers as,
 * in general, mCtrl interface should be more self-contained.
 *
 * Therefore this headers defines some constants and types which are equivalent
 * to those in @c <commctrl.h>, with name beginning with a prefix @c MC_.
 * Note that applications including both mCtrl headers and @c <commctrl.h> can
 * use those constants and types interchangeably as the constants and types are
 * binary compatible.
 *
 * Note that documentation of the @c <commctrl.h> replacements is quite brief.
 * Refer to MSDN documentation of their counterparts for more detailed info.
 *
 * Additionally this header provides miscellaneous constants marking reserved
 * ranges for messages and notifications of mCtrl controls (in a similar manner
 * as @c <commctrl.h> does for @c COMCTL32.DLL controls).
 */


/**
 * @name Common Constants
 */
/*@{*/

/** @brief Equivalent of @c CLR_NONE from @c <commctrl.h>. */
#define MC_CLR_NONE              ((COLORREF) 0xFFFFFFFF)
/** @brief Equivalent of @c CLR_DEFAULT from @c <commctrl.h>. */
#define MC_CLR_DEFAULT           ((COLORREF) 0xFF000000)

/** @brief Equivalent of @c I_IMAGECALLBACK from @c <commctrl.h>. */
#define MC_I_IMAGECALLBACK       (-1)
/** @brief Equivalent of @c I_IMAGENONE from @c <commctrl.h>. */
#define MC_I_IMAGENONE           (-2)

/** @brief Equivalent of @c I_CHILDRENCALLBACK from @c <commctrl.h>. */
#define MC_I_CHILDRENCALLBACK    (-1)

/** @brief Equivalent of @c LPSTR_TEXTCALLBACKW from @c <commctrl.h> (Unicode variant). */
#define MC_LPSTR_TEXTCALLBACKW   ((LPWSTR)(INT_PTR) -1)
/** @brief Equivalent of @c LPSTR_TEXTCALLBACKA from @c <commctrl.h> (ANSI variant). */
#define MC_LPSTR_TEXTCALLBACKA   ((LPSTR)(INT_PTR) -1)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** @brief Equivalent of @c LPSTR_TEXTCALLBACK from @c <commctrl.h>.
 *  @details Unicode-resolution alias.
 *  @sa MC_LPSTR_TEXTCALLBACKW MC_LPSTR_TEXTCALLBACKA */
#define MC_LPSTR_TEXTCALLBACK    MCTRL_NAME_AW(MC_LPSTR_TEXTCALLBACK)

/*@}*/


/**
 * @name Common Types
 */
/*@{*/

/**
 * @brief Equivalent of @c NMCUSTOMDRAWINFO from @c <commctrl.h>.
 *
 * For more detailed information refer to documentation of @c NMCUSTOMDRAWINFO
 * on MSDN.
 */
typedef struct MC_NMCUSTOMDRAWINFO_tag {
    NMHDR hdr;
    DWORD dwDrawStage;
    HDC hdc;
    RECT rc;
    DWORD_PTR dwItemSpec;
    UINT  uItemState;
    LPARAM lItemlParam;
} MC_NMCUSTOMDRAW;

/*@}*/


/**
 * @name Control Specific Message Ranges
 */
/*@{*/

#define MC_EXM_FIRST        (WM_USER+0x4000 + 0)
#define MC_EXM_LAST         (WM_USER+0x4000 + 49)

#define MC_GM_FIRST         (WM_USER+0x4000 + 50)
#define MC_GM_LAST          (WM_USER+0x4000 + 199)

#define MC_HM_FIRST         (WM_USER+0x4000 + 200)
#define MC_HM_LAST          (WM_USER+0x4000 + 299)

#define MC_MTM_FIRST        (WM_USER+0x4000 + 300)
#define MC_MTM_LAST         (WM_USER+0x4000 + 399)

#define MC_MBM_FIRST        (WM_USER+0x4000 + 400)
#define MC_MBM_LAST         (WM_USER+0x4000 + 499)

#define MC_PVM_FIRST        (WM_USER+0x4000 + 500)
#define MC_PVM_LAST         (WM_USER+0x4000 + 599)

#define MC_CHM_FIRST        (WM_USER+0x4000 + 600)
#define MC_CHM_LAST         (WM_USER+0x4000 + 699)

#define MC_TLM_FIRST        (WM_USER+0x4000 + 700)
#define MC_TLM_LAST         (WM_USER+0x4000 + 799)

#define MC_IVM_FIRST        (WM_USER+0x4000 + 800)
#define MC_IVM_LAST         (WM_USER+0x4000 + 849)

/*@}*/


/**
 * @name Control Specific Notification Ranges
 */
/*@{*/

#define MC_EXN_FIRST        (0x40000000 + 0)
#define MC_EXN_LAST         (0x40000000 + 49)

#define MC_GN_FIRST         (0x40000000 + 100)
#define MC_GN_LAST          (0x40000000 + 199)

#define MC_HN_FIRST         (0x40000000 + 200)
#define MC_HN_LAST          (0x40000000 + 299)

#define MC_MTN_FIRST        (0x40000000 + 300)
#define MC_MTN_LAST         (0x40000000 + 349)

#define MC_PVN_FIRST        (0x40000000 + 400)
#define MC_PVN_LAST         (0x40000000 + 499)

#define MC_CHN_FIRST        (0x40000000 + 500)
#define MC_CHN_LAST         (0x40000000 + 599)

#define MC_TLN_FIRST        (0x40000000 + 600)
#define MC_TLN_LAST         (0x40000000 + 699)

/*@}*/


/*PSIPHON*/
/* Converting mCtrl from a dynamic to static library requires us to do some
additional initialization. These functions must be called once each, before
(and after) any other mCtrl module initialization or termination.*/
void mc_StaticLibInitialize();
void mc_StaticLibTerminate();
/*/PSIPHON*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_COMMON_H */
