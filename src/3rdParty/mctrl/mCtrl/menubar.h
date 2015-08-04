/*
 * Copyright (c) 2012-2013 Martin Mitas
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

#ifndef MCTRL_MENUBAR_H
#define MCTRL_MENUBAR_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Menu bar control (@c MC_WC_MENUBAR).
 *
 * The @c MC_WC_MENUBAR is implantation of a control generally known as
 * Internet Explorer-Style Menu Bar. It is a control which can host a menu
 * (here represented by a menu handle, @c HMENU), but which generally works
 * as a toolbar.
 *
 * The standard menus take whole width of the window for their menubars,
 * and there can only be used one menu in a top-level windows. Child windows
 * can not have a menu at all (well, we are not talking about pop-up menus).
 *
 * The @c MC_WC_MENUBAR offers solution to these limitations. It has been
 * designed especially with following use cases in mind:
 *
 * - Embedding the menu into a standard ReBar control from @c COMCTL32.DLL.
 *
 * - Positioning the control on other position or with different size then what
 *   is normally enforced for normal menu of a window or dialog.
 *
 * - Possibility to create this control in child windows, or having multiple
 *   menubars in a single window.
 *
 *
 * @section sec_mb_subclass Superclassed Tool Bar
 *
 * Actually the @c MC_WC_MENUBAR is implemented as a superclass of the standard
 * toolbar (from @c COMCTL32.DLL) control, so you can use its style, and also
 * some toolbar messages.
 *
 * Of course there are also differences: The menubar control automatically sets
 * some toolbar styles when created, as it sees fit for its purpose. Application
 * still can reset it with @c SetWindowLong and @c GWL_STYLE.
 *
 * Furthermore the menubar control does not support toolbar messages which add,
 * modify or remove toolbar buttons. The control just manages them
 * automatically to reflect the installed menu.
 *
 * I.e. sending any of these toolbar messages to the control always fails:
 * - @c TB_ADDBITMAP
 * - @c TB_ADDSTRING
 * - @c TB_ADDBUTTONS
 * - @c TB_BUTTONSTRUCTSIZE
 * - @c TB_CHANGEBITMAP
 * - @c TB_CUSTOMIZE
 * - @c TB_DELETEBUTTON
 * - @c TB_ENABLEBUTTON
 * - @c TB_HIDEBUTTON
 * - @c TB_INDETERMINATE
 * - @c TB_INSERTBUTTON
 * - @c TB_LOADIMAGES
 * - @c TB_MARKBUTTON
 * - @c TB_MOVEBUTTON
 * - @c TB_PRESSBUTTON
 * - @c TB_REPLACEBITMAP
 * - @c TB_SAVERESTORE
 * - @c TB_SETANCHORHIGHLIGHT
 * - @c TB_SETBITMAPSIZE
 * - @c TB_SETBOUNDINGSIZE
 * - @c TB_SETCMDID
 * - @c TB_SETDISABLEDIMAGELIST
 * - @c TB_SETHOTIMAGELIST
 * - @c TB_SETIMAGELIST
 * - @c TB_SETINSERTMARK
 * - @c TB_SETPRESSEDIMAGELIST
 * - @c TB_SETSTATE
 *
 *
 * @section sec_mb_create Installing a Menu
 *
 * To install a menu in the menubar, you may set parameter @c lpParam of
 * @c CreateWindow() or @c CreateWindowEx() to the handle of the menu (@c HMENU).
 * Or, after the menubar is created, you may install a menu with the message
 * @ref MC_MBM_SETMENU.
 *
 * Either way the application is responsible to keep the menu handle valid
 * as long as the menubar exists (or until other menu is installed in the
 * menubar).
 *
 * Note however that changes to the menu are not automatically reflected in the
 * menubar. If application programatically changes top-level items of the menu
 * (for example add new pop-ups, disable some of them etc.), it then has to
 * send @ref MC_MBM_REFRESH to reflect the changes.
 *
 *
 * @section sec_mb_notifications Notifications
 *
 * The control sends notifications of both, the toolbar and menu.
 *
 * To handle the actions corresponding to the menu items, application
 * uses the notification @c WM_COMMAND as with a normal menu. It can also
 * take use of @c WM_MENUSELECT and @c WM_INITMENU.
 *
 * Toolbar notifications are sent through @c WM_NOTIFY. For example,
 * @c TBN_DROPDOWN or @c TBN_HOTITEMCHANGE are sent as any other notifications
 * normal toolbar fires.
 *
 * All the notifications are sent by default to a window which was parent of
 * the menubar when creating the menubar. One exception is if the parent is
 * a ReBar control: Because it will often be the case and the ReBar control
 * cannot handle the notifications properly, they are then sent to the
 * grand-father of the menubar (i.e. parent of the ReBar).
 *
 * Application can also explicitly set the target window of the notifications
 * with the standard toolbar message @c TB_SETPARENT.
 *
 *
 * @section sec_mb_hotkeys Hot Keys
 *
 * To work as intended, the control requires some cooperation with the
 * application. The message loop in the application should call
 * the function @c mcIsMenubarMessage to handle hot keys of the menu items
 * and allow to activate the menu with the key @c F10.
 *
 * Hence code of the message loop in applications using the menubar control
 * should be similar to the example below:
 *
 * @code
 * MSG msg;
 *
 * while(GetMessage(&msg, NULL, 0, 0)) {
 *     if(TranslateAccelerator(hWnd, hAccel, &msg))
 *         continue;
 *     if(mcIsMenubarMessage(hwndMenubar, &msg))
 *         continue;
 *     if(IsDialogMessage(hWnd, &msg))
 *         continue;
 *
 *     TranslateMessage(&msg);
 *     DispatchMessage(&msg);
 * }
 * @endcode
 *
 *
 * @section sec_mb_in_rebar Embedding in a ReBar Control
 *
 * Application may often need to embed the menubar control in a ReBar control.
 * To do so, the application developer need to embed the menubar control
 * in a Rebar control band. That is usually performed with a code similar to
 * this one:
 *
 * @code
 * REBARBANDINFO band = {0};
 * DWORD dwBtnSize;
 *
 * band.cbSize = sizeof(REBARBANDINFO);
 * band.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_ID;
 * band.fStyle = RBBS_GRIPPERALWAYS | RBBS_TOPALIGN | RBBS_VARIABLEHEIGHT;
 * band.hwndChild = hwndMenubar;
 * dwBtnSize = (DWORD)SendMessage(band.hwndChild, TB_GETBUTTONSIZE, 0,0);
 * band.cyChild = HIWORD(dwBtnSize);
 * band.cxMinChild = 0;
 * band.cyMinChild = HIWORD(dwBtnSize);
 * band.cyMaxChild = HIWORD(dwBtnSize);
 * band.cyIntegral = HIWORD(dwBtnSize);
 * band.cx = 240;  // or other value as appropriate for the menu and dialog window size
 * band.wID = BAND_MENUBAR;  // band ID identifying the one with the menubar
 * SendMessage(hwndRebar, RB_INSERTBAND, -1, (LPARAM) &band);
 * @endcode
 *
 * If the application developer desires to support chevron on the band hosting
 * the menubar, additional steps are required:
 *
 * -# Application developer should consider using the extended toolbar style
 * @c TBSTYLE_EX_HIDECLIPPEDBUTTONS:
 * @code
 * SendMessage(hwndMenubar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_HIDECLIPPEDBUTTONS);
 * @endcode
 * -# The @c band.fStyle in the code above has to specify @c RBBS_USECHEVRON in addition.
 * -# The member @c band.cxIdeal has to be initialized in the @c REBARBANDINFO above,
 * before inserting the new band:
 * @code
 * SIZE szIdeal;
 *
 * band.fMask |= RBBIM_IDEALSIZE;
 * band.cxIdeal = szIdeal.cx;
 * SendMessage(hwndMenubar, TB_GETIDEALSIZE, FALSE, (LPARAM) &szIdeal);
 * @endcode
 * -# The dialog procedure has to handle @c RBN_CHEVRONPUSHED notification from
 * the rebar control. If the notification is about the band hosting a menubar,
 * the function @c mcMenubar_HandleRebarChevronPushed() should be called in
 * response.
 * @code
 * ...
 * case WM_NOTIFY:
 * {
 *     NMHDR* hdr = (NMHDR*) lParam;
 *     if(hdr->hwndFrom == hwndRebar  &&  hdr->code == RBN_CHEVRONPUSHED) {
 *         NMREBARCHEVRON* nm = (NMREBARCHEVRON*) hdr;
 *         if(nm->wID == BAND_MENUBAR)
 *             mcMenubar_HandleRebarChevronPushed(hwndMenubar, nm);
 *             break;
 *         }
 *     }
 *     break;
 * }
 * ...
 * @endcode
 *
 *
 * @section std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c CCM_SETNOTIFYWINDOW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcMenubar_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcMenubar_Terminate(void);

/*@}*/


/**
 * @name Related Functions
 */
/*@{*/

/**
 * @brief Determines whether a message is intended for the specified
 * menubar control and, if it is, processes the message.
 *
 * The application typically calls this function in main message loop.
 *
 * @param hwndMenubar The menubar control.
 * @param lpMsg The message.
 * @return @c TRUE, if the message has been processed; @c FALSE otherwise.
 */
BOOL MCTRL_API mcIsMenubarMessage(HWND hwndMenubar, LPMSG lpMsg);


#ifndef DOXYGEN
typedef struct tagNMREBARCHEVRON NMREBARCHEVRON;
#endif

/**
 * @brief Helper function for ReBar chevron support.
 *
 * Application can embed the menubar control in a ReBar control. In such case
 * the application may want to support the chevron (ReBar window style @c
 * RBBS_USECHEVRON).
 *
 * In such case the application gets the notification @c RBN_CHEVRONPUSHED
 * whenever the ReBar band is too small for complete menubar and user clicks
 * the chevron button. In such case the application should propagate the
 * notification (when about the band with the menubar) to this function,
 * which creates and opens a pop-up for all the menu items not visible due
 * the insufficient space.
 *
 * In case of success (the function returns @c TRUE), the function returns
 * only after the chevron popup menu was closed.
 *
 * @param hwndMenubar The menubar control, or @c NULL. If menubar handle is
 * provided, the function verifies that the notification is about the menubar
 * and returns @c FALSE if it is not. When @c NULL, it is responsibility of the
 * application to ensure the notification is about the band hosting a menubar
 * control.
 * @param lpRebarChevron Pointer to the structure @c NMREBARCHEVRON associated
 * with the @c RBN_CHEVRONPUSHED notification.
 * @return @c TRUE if the pop-up menu for the chevron button has been created
 * and opened, @c FALSE otherwise.
 *
 * @attention If @c hwndMenubar is @c NULL and the @c lpRebarChevron is
 * notification about a ReBar band which hosts other window then a menubar
 * control, the application behavior is undefined. The application may crash.
 */
BOOL MCTRL_API mcMenubar_HandleRebarChevronPushed(HWND hwndMenubar,
                                                  NMREBARCHEVRON* lpRebarChevron);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_MENUBARW       L"mCtrl.menubar"
/** Window class name (ANSI variant). */
#define MC_WC_MENUBARA        "mCtrl.menubar"

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Install a menu into the menubar.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c HMENU) The menu to install.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MBM_SETMENU            (MC_MBM_FIRST + 0)

/**
 * @brief Updates the menubar to reflect changes in the installed menu.
 *
 * Application has to send this messages after it changes the top-level menu
 * items (e.g. adds or deletes a sub-menu, enables or disables a sub-menu etc.).
 *
 * Note that it is needed only if the top-level items changes (i.e. the items
 * directly visible on the horizontal menubar. Contents of pop-up menus can
 * be changed without sending this notification.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MBM_REFRESH            (MC_MBM_FIRST + 1)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_MENUBARW MC_WC_MENUBARA */
#define MC_WC_MENUBAR         MCTRL_NAME_AW(MC_WC_MENUBAR)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_MENUBAR_H */
