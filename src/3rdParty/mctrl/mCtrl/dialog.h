/*
 * Copyright (c) 2012 Martin Mitas
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

#ifndef MCTRL_DIALOG_H
#define MCTRL_DIALOG_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Dialog functions
 *
 * This module offers functions for creation of modal and modeless dialogs
 * in very similar manner as standard functions @c DialogBox() and
 * @c CreateDialog() do.
 *
 * Therefore mCtrl functions are very similar to their @c USER32.DLL
 * counterparts, including their function name and parameters. Actually
 * the only difference is that the mCtrl functions take an extra argument
 * @c dwFlags. When the @c dwFlags is zero, the functions behave exactly as
 * the original functions.
 *
 * When set to non-zero, the functions provide new functionality. Currently
 * only the flag @ref MC_DF_DEFAULTFONT is supported. When set, it forces the
 * dialog to use default font, as defined by MS user interface guide lines.
 *
 * All the functions support the classic dialog templates (@c DLGTEMPLATE)
 * as well as the extended dialog templates (@c DLGTEMPLATEEX).
 */

/**
 * @name Dialog flags
 */
/*@{*/

/**
 * @brief Force a default font into the dialog template.
 *
 * When this flag is set, the dialog template is modified so the dialog uses
 * a default font for the particular Windows version, according to the
 * MS user interface guide lines.
 *
 * Depending on Windows version, it forces the template to use
 * <tt>MS Shell Dlg</tt>, <tt>MS Shell Dlg 2</tt> or <tt>Segoe UI</tt>.
 *
 * Note that when using this flag, the font specified originally in the
 * dialog template is used only as a fallback in case of any error.
 *
 * @attention Metrics of the default fonts does differ. When using this font
 * you should test your dialog on multiple Windows version to ensure that no
 * content overflows.
 */
#define MC_DF_DEFAULTFONT              0x00000001

/*@}*/


/**
 * @name Modeless dialog functions
 */
/*@{*/

/**
 * Creates modeless dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * Creates modeless dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogW(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags) \
                mcCreateDialogParamW((hInstance),(lpTemplateName),(hWndParent),   \
                                     (lpDialogFunc),0L,(dwFlags))

/**
 * Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogA(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags) \
                mcCreateDialogParamA((hInstance),(lpTemplateName),(hWndParent),   \
                                     (lpDialogFunc),0L,(dwFlags))

/**
 * Creates modeless dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogIndirectParamW(HINSTANCE hInstance,
                LPCDLGTEMPLATEW lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogIndirectParamA(HINSTANCE hInstance,
                LPCDLGTEMPLATEA lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * Creates modeless dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogIndirectW(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)  \
                mcCreateDialogIndirectParamW((hInstance),(lpTemplate),(hWndParent),    \
                                             (lpDialogFunc),0L,(dwFlags))

/**
 * Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogIndirectA(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)  \
                mcCreateDialogIndirectParamA((hInstance),(lpTemplate),(hWndParent),    \
                                             (lpDialogFunc),0L,(dwFlags))

/*@}*/


/**
 * @name Modal dialog functions
 */
/*@{*/

/**
 * Creates and runs modal dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * Creates and runs modal dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxW(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxParamW((hInstance),(lpTemplateName),(hWndParent),     \
                                  (lpDialogFunc),0L,(dwFlags))

/**
 * Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxA(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxParamA((hInstance),(lpTemplateName),(hWndParent),     \
                                  (lpDialogFunc),0L,(dwFlags))


/**
 * Creates and runs modal dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxIndirectParamW(HINSTANCE hInstance,
                LPCDLGTEMPLATEW lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxIndirectParamA(HINSTANCE hInstance,
                LPCDLGTEMPLATEA lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * Creates and runs modal dialog (Unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxIndirectW(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxIndirectParamW((hInstance),(lpTemplate),(hWndParent),     \
                                          (lpDialogFunc),0L,(lpDialogFunc))

/**
 * Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxIndirectA(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxIndirectParamA((hInstance),(lpTemplate),(hWndParent),     \
                                          (lpDialogFunc),0L,(lpDialogFunc))

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa mcCreateDialogParamW mcCreateDialogParamA */
#define mcCreateDialogParam            MCTRL_NAME_AW(mcCreateDialogParam)
/** Unicode-resolution alias. @sa mcCreateDialogW mcCreateDialogA */
#define mcCreateDialog                 MCTRL_NAME_AW(mcCreateDialog)
/** Unicode-resolution alias. @sa mcCreateDialogIndirectParamW mcCreateDialogIndirectParamA */
#define mcCreateDialogIndirectParam    MCTRL_NAME_AW(mcCreateDialogIndirectParam)
/** Unicode-resolution alias. @sa mcCreateDialogIndirectW mcCreateDialogIndirectA */
#define mcCreateDialogIndirect         MCTRL_NAME_AW(mcCreateDialogIndirect)
/** Unicode-resolution alias. @sa mcDialogBoxParamW mcDialogBoxParamA */
#define mcDialogBoxParam               MCTRL_NAME_AW(mcDialogBoxParam)
/** Unicode-resolution alias. @sa mcDialogBoxW mcDialogBoxA */
#define mcDialogBox                    MCTRL_NAME_AW(mcDialogBox)
/** Unicode-resolution alias. @sa mcDialogBoxIndirectParamW mcDialogBoxIndirectParamA */
#define mcDialogBoxIndirectParam       MCTRL_NAME_AW(mcDialogBoxIndirectParam)
/** Unicode-resolution alias. @sa mcDialogBoxIndirectW mcDialogBoxIndirectA */
#define mcDialogBoxIndirect            MCTRL_NAME_AW(mcDialogBoxIndirect)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_DIALOG_H */
