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

#ifndef MCTRL_IMGVIEW_H
#define MCTRL_IMGVIEW_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Image view control (@c MC_WC_IMGVIEW).
 *
 * Image view control is specialized control for displaying an image. Unlike
 * the standard @c STATIC control which can only support bitmaps and icons,
 * the image view control supports more image formats: BMP, ICON, GIF, JPEG,
 * PNG, TIFF, WMF and EMF.
 *
 * @attention The image view control requires @c GDIPLUS.DLL version 1.0 or
 * newer to work correctly. This library was introduced in Windows XP and
 * Windows Server 2003. If your application needs to use this control on Windows
 * 2000, you may need to distribute @c GDIPLUS.DLL along with your application.
 * (Microsoft released @c GDIPLUS.DLL 1.0 as a redistributable for this purpose.)
 *
 *
 * @section sect_imgview_creation Control Creation
 *
 * The control can display images loaded from a file as well as images embedded
 * as resources in a DLL or EXE module. Note that window text as passed into
 * @c CreateWindow() is interpreted as a name of a resource in the same module
 * as specified by the @ HMODULE handle passed into the function. It may also
 * specify the integer ID of a resource with the @c MAKEINTRESOURCE, or in the
 * form "#123".
 *
 * This allows to create the control and associate an image directly in the
 * resource script:
 * @code
 * 50 RCDATA path/to/image1.png
 * 51 RCDATA path/to/image2.jpg
 * "imgname" RCDATA path/to/image3.bmp
 *
 * IDD_DIALOG DIALOG 100, 100, 74, 150
 * STYLE WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME
 * EXSTYLE WS_EX_DLGMODALFRAME
 * CAPTION "mCtrl Example: IMGVIEW Control"
 * FONT 8, "MS Shell Dlg"
 * BEGIN
 *     CONTROL 50, IDC_IMGVIEW_PNG, MC_WC_IMGVIEW, 0, 7, 7, 16, 16, WS_EX_STATICEDGE
 *     CONTROL "#51", IDC_IMGVIEW_PNG, MC_WC_IMGVIEW, 0, 30, 7, 16, 16, WS_EX_STATICEDGE
 *     CONTROL "imgname", IDC_IMGVIEW_PNG, MC_WC_IMGVIEW, 0, 51, 7, 16, 16, WS_EX_STATICEDGE
 * END
 * @endcode
 *
 * Note the control only looks for image resources of the following resource
 * types: @c RT_RCDATA, @c RT_BITMAP (bitmaps only), "PNG" (PNG only) or @c RT_HTML.
 * (The last option is the application uses the image also with the
 * @c MC_WC_HTML control).
 *
 *
 * @section sect_imgview_setting Setting the Image
 *
 * It is also possible to set the image in the application run time.
 *
 * Use message @ref MC_IVM_LOADRESOURCE to set the image from the resource
 * of a DLL or EXE module. Note the application is responsible to ensure
 * the module is not unloaded while the image is in use by the control
 * I.e. until the control is associated with different image, or until the
 * control is destroyed.
 *
 * To load the image from a file, use the message @ref MC_IVM_LOADFILE.
 *
 *
 * @section sect_imgview_scaling Image Scaling
 *
 * By default, the image is scaled so that its aspect ratio is preserved,
 * and as much of the control area is utilized as possible.
 *
 * Application can change this behavior by specifying the style
 * @ref MC_IVS_REALSIZECONTROL, which scales the image to the size of the
 * control without preserving the aspect ratio; or by the style
 * @ref MC_IVS_REALSIZEIMAGE which suppresses the scaling altogether.
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcImgView_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcImgView_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_IMGVIEWW       L"mCtrl.imgView"
/** Window class name (ANSI variant). */
#define MC_WC_IMGVIEWA        "mCtrl.imgView"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief When set, the control background is transparent. */
#define MC_IVS_TRANSPARENT           0x00000001

/** @brief When set, the image is scaled to dimensions of the control.
 *  @note This style cannot be used together with @ref MC_IVS_REALSIZEIMAGE.
 */
#define MC_IVS_REALSIZECONTROL       0x00000100
/** @brief When set, the image is painted in its original dimensions.
 *  @details If the control is too small, only part of the image is painted.
 *  @note This style cannot be used together with @ref MC_IVS_REALSIZECONTROL.
 */
#define MC_IVS_REALSIZEIMAGE         0x00000200

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Load image from a resource (Unicode variant).
 * @param[in] wParam (@c HINSTANCE) Module providing the resource.
 * @param[in] lParam (@c const @c WCHAR*) Resource name.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @note The message can be used to reset the control so it does not display
 * any image, if both parameters are set to zero.
 */
#define MC_IVM_LOADRESOURCEW        (MC_IVM_FIRST + 0)

/**
 * @brief Load image from a resource (ANSI variant).
 * @param[in] wParam (@c HINSTANCE) Module providing the resource.
 * @param[in] lParam (@c const @c char*) Resource name.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @note The message can be used to reset the control so it does not display
 * any image, if both parameters are set to zero.
 */
#define MC_IVM_LOADRESOURCEA        (MC_IVM_FIRST + 1)

/**
 * @brief Load image from a file (Unicode variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) File path.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @note The message can be used to reset the control so it does not display
 * any image, if both parameters are set to zero.
 */
#define MC_IVM_LOADFILEW            (MC_IVM_FIRST + 2)

/**
 * @brief Load image from a file (ANSI variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) File path.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @note The message can be used to reset the control so it does not display
 * any image, if both parameters are set to zero.
 */
#define MC_IVM_LOADFILEA            (MC_IVM_FIRST + 3)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_IMGVIEWW MC_WC_IMGVIEWA */
#define MC_WC_IMGVIEW         MCTRL_NAME_AW(MC_WC_IMGVIEW)
/** Unicode-resolution alias. @sa MC_IVM_LOADRESOURCEW MC_IVM_LOADRESOURCEA */
#define MC_IVM_LOADRESOURCE   MCTRL_NAME_AW(MC_IVM_LOADRESOURCE)
/** Unicode-resolution alias. @sa MC_IVM_LOADFILEW MC_IVM_LOADFILEA */
#define MC_IVM_LOADFILE       MCTRL_NAME_AW(MC_IVM_LOADFILE)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_IMGVIEW_H */
