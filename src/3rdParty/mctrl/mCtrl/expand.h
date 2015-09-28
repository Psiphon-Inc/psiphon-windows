/*
 * Copyright (c) 2012-2013 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MCTRL_EXPAND_H
#define MCTRL_EXPAND_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Expand control (@c MC_WC_EXPAND).
 *
 * The expand control is a utility control used to toggle size of another window
 * between &quot;collapsed&quot; and &quot;expanded&quot; states.
 *
 * In addition, when collapsing, the control automatically disables and hides
 * all child windows of the managed window which fall out of the visible area;
 * and when expanding it enables and shows all the child windows revealed by the
 * resize operation. Application can disable this behavior with the style
 * @ref MC_EXS_IGNORECHILDREN.
 *
 *
 * @section Managed Window
 *
 * By default the managed window is a parent window of the control. Use message
 * @c CCM_SETNOTIFYWINDOW to change what window the control manages.
 *
 * @note The root control also gets all the notifications from the control.
 *
 *
 * @section Expanded and Collapsed Sizes
 *
 * If both width and height the of the expanded and/or collapsed sizes is zero,
 * the control computes automatically from analyzes of position and size of
 * all child windows of the managed window (usually the parent).
 *
 * The width of the managed window is kept unchanged in this automatic regime.
 * The height of the managed window is calculated so that all child windows
 * of the managed window are visible (the expanded state) or all child windows
 * positioned below the expand control itself are hidden (the collapsed state).
 *
 * If you use style @ref MC_EXS_CACHESIZES, the computed sizes are retained
 * for next use, i.e. the sizes are computed only once.
 *
 * You can set expanded and collapsed sizes explicitly with message
 * @ref MC_EXM_SETCOLLAPSEDSIZE and @ref MC_EXM_SETEXPANDEDSIZE respectively.
 *
 *
 * @section Initial State
 *
 * After creation of the control, its logical state is collapsed. Though
 * the control does not resize the dialog because it may not be already fully
 * initialized, e.g. if the expand control is not last control created in the
 * dialog.
 *
 * Therefore it is supposed that application explicitly sends the message
 * @ref MC_EXM_EXPAND after the dialog and all its children are created,
 * usually as part of the @c WM_INITDIALOG handling.
 *
 *
 * These standard messages are handled by the control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_SETNOTIFYWINDOW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 * - @c NM_RELEASEDCAPTURE
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcExpand_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcExpand_Terminate(void);


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_EXPANDW          L"mCtrl.expand"
/** Window class name (ANSI variant). */
#define MC_WC_EXPANDA           "mCtrl.expand"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/**
 * @brief Cache expanded and collapsed sizes.
 *
 * When in the automatic mode, this style allows the control to store expanded
 * and/or collapsed sizes instead of recomputing it each time its state changes.
 */
#define MC_EXS_CACHESIZES           0x0001

/**
 * @brief Expanded and collapsed sizes are interpreted for whole window.
 *
 * If this style is not set, they determine client size only.
 */
#define MC_EXS_RESIZEENTIREWINDOW   0x0002

/**
 * @brief Enable painting with double buffering.
 *
 * It prevents flickering when the control is being continuously resized.
 */
#define MC_EXS_DOUBLEBUFFER         0x0004

/**
 * @brief Causes the control changes size of parent window using an animation.
 *
 * Using this style causes the control will send series of WM_SIZE messages
 * to the parent window in a period of the animation time, gradually changing
 * its size to the desired values.
 *
 * Application has to take this into account as if there are other means which
 * change the window size, they can interfere.
 */
#define MC_EXS_ANIMATE              0x0008

/**
 * @brief Instructs the control to not change state of children of the managed
 * window.
 *
 * When not set, it may show/hide and enable/disable child windows of the
 * managed window, whenever they are (un)covered by the managed window
 * resizing.
 *
 * However that might interfere with application's logic about
 * enabling/disabling the controls.
 *
 * If you use this style, application should make disable the children manually
 * in the response to control notification @ref MC_EXN_EXPANDING or
 * @ref MC_EXN_EXPANDED.
 */
#define MC_EXS_IGNORECHILDREN       0x0010

/*@}*/


/**
 * @name Flags for MC_EXM_EXPAND and MC_EXM_TOGGLE
 * @anchor MC_EXE_xxxx
 */
/*@{*/

/** @brief Perform the expand/collapse without using an animation. */
#define MC_EXE_NOANIMATE           (1 << 0)

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Specify size of parent's client area when in collapsed state.
 *
 * If you set both width and height to zero, then the size is computed
 * automatically from position of all controls in the dialog.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c DWORD) The size. Low-order word specifies width and
 * high-order word specifies height.
 * @return (@c DWORD) The original size.
 */
#define MC_EXM_SETCOLLAPSEDSIZE    (MC_EXM_FIRST + 0)

/**
 * @brief Gets size of parent's client area when in collapsed state.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c DWORD) The size. Low-order word specifies width and high-order
 * word specifies height.
 */
#define MC_EXM_GETCOLLAPSEDSIZE    (MC_EXM_FIRST + 1)

/**
 * @brief Specify size of parent's client area when in expanded state.
 *
 * If you set both width and height to zero, then the size is computed
 * automatically from position of all controls in the dialog.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c DWORD) The size. Low-order word specifies width and
 * high-order word specifies height.
 * @return (@c DWORD) The original size.
 */
 #define MC_EXM_SETEXPANDEDSIZE     (MC_EXM_FIRST + 2)

/**
 * @brief Gets size of parent's client area when in expanded state.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c DWORD) The size. Low-order word specifies width and high-order
 * word specifies height.
 */
#define MC_EXM_GETEXPANDEDSIZE     (MC_EXM_FIRST + 3)

/**
 * @brief Sets current state of the control to expanded or collapsed.
 *
 * @param[in] wParam (@c BOOL) Set to @c TRUE to expand, @c FALSE to collapse.
 * @param[in] lParam (@c DWORD) Flags. See @ref MC_EXE_xxxx.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_EXM_EXPAND              (MC_EXM_FIRST + 4)

/**
 * @brief Toggles current state of the control between expanded and collapsed.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c DWORD) Flags. See @ref MC_EXE_xxxx.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_EXM_TOGGLE              (MC_EXM_FIRST + 5)

/**
 * @brief Gets current state of the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE if expanded, @c FALSE if collapsed.
 */
#define MC_EXM_ISEXPANDED          (MC_EXM_FIRST + 6)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when the control begins expansing or collapsing the parent
 * window.
 *
 * When application receives the message, the control is logically already in
 * the desired style, so it can ask about it using @ref MC_EXM_ISEXPANDED.
 *
 * However the size of the parent window may be different from the desired
 * state if the control animates it, as that takes some time.
 * When the parent resizing is also finished, the application will receive
 * notification @ref MC_EXN_EXPANDED.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c NMHDR*)
 * @return Application should return zero if it processes the notification.
 */
#define MC_EXN_EXPANDING           (MC_EXN_FIRST + 0)

/**
 * @brief Fired after the control has finished resizing of the parent window.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c NMHDR*)
 * @return Application should return zero if it processes the notification.
 */
#define MC_EXN_EXPANDED            (MC_EXN_FIRST + 1)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_EXPANDW MC_WC_EXPANDA */
#define MC_WC_EXPAND           MCTRL_NAME_AW(MC_WC_EXPAND)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_EXPAND_H */
