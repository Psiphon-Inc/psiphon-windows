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


#ifndef MCTRL_THEME_H
#define MCTRL_THEME_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#include <vssym32.h>
#include <vsstyle.h>
#include <uxtheme.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Theme wrapper functions
 *
 * This miscellaneous module provides wrapper of functions exported from
 * @c UXTHEME.DLL, as the library is only available on Windows XP and later.
 *
 * @note mCtrl only uses @c UXTHEME.DLL if it is convinced the application is
 * themed. I.e. it must be linked against @c COMCTL32.DLL version 6 or later,
 * as earlier versions does not support themed controls and mCtrl tries to be
 * consistent with rest of the application.
 *
 * The wrapper functions provided by this module simply have the same name
 * as functions exported from @c UXTHEME.DLL, with the prefix <tt>mc</tt>
 * prepended. Each wrapper simply calls its counterpart in @c UXTHEME.DLL
 * if it is loaded and available (as some @c UXTHEME.DLL functions were
 * introduced later then on Windows XP).
 *
 * If the @c UXTHEME.DLL is not used, or if the particular function is not
 * available, then most of the wrapper functions just fail gracefully and
 * return @c E_NOTIMPL, @c NULL, @c 0 or @c FALSE, depending on the return type.
 *
 * However there are also wrapper functions which provide some fallback
 * implementation. Those cases are described in description of particular
 * functions.
 *
 * @note Note that future versions of mCtrl can provide fallback implementation
 * for more functions. If you want directly call the @c UXTHEME.DLL function,
 * then get its address manually with @c GetProcAddress().
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Initializes the module. This function must be called before any other
 * function of this module is used.
 *
 * Note that the function checks version of Windows and version of
 * <tt>COMCTL32.DLL</tt>. It only loads <tt>UXTHEME.DLL</tt> and gets
 *
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcTheme_Initialize(void);

/**
 * Uninitialization. If @ref mcTheme_Initialize() loaded @c UXTHEME.DLL, is is
 * unloaded with @c FreeLibrary() and releases any related resources.
 */
void MCTRL_API mcTheme_Terminate(void);

/*@}*/


/**
 * @name Wrapper Functions
 */
/*@{*/

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c BeginBufferedAnimation() if available (and @c UXTHEME.DLL is in use),
 * or returns @c NULL.
 *
 * @param hwnd
 * @param hdcTarget
 * @param rcTarget
 * @param dwFormat
 * @param pPaintParams
 * @param pAnimationParams
 * @param phdcFrom
 * @param phdcTo
 * @return Return value of @c BeginBufferedAnimation() or @c NULL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HANIMATIONBUFFER MCTRL_API mcBeginBufferedAnimation(HWND hwnd, HDC hdcTarget,
            const RECT* rcTarget, BP_BUFFERFORMAT dwFormat,
            BP_PAINTPARAMS* pPaintParams, BP_ANIMATIONPARAMS* pAnimationParams,
            HDC* phdcFrom, HDC* phdcTo);

/**
 * Calls @c BeginBufferedPaint() if available (and @c UXTHEME.DLL is in use),
 * or returns @c NULL.
 *
 * @param hdcTarget
 * @param prcTarget
 * @param dwFormat
 * @param pPaintParams
 * @param phdc
 * @return Return value of @c BeginBufferedPaint() or @c NULL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HPAINTBUFFER MCTRL_API mcBeginBufferedPaint(HDC hdcTarget, const RECT* prcTarget,
            BP_BUFFERFORMAT dwFormat, BP_PAINTPARAMS* pPaintParams, HDC* phdc);

/**
 * Calls @c BeginPanningFeedback() if available (and @c UXTHEME.DLL is in use),
 * or returns @c FALSE.
 *
 * @param hwnd
 * @return Return value of @c BeginPanningFeedback() or @c FALSE.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
BOOL MCTRL_API mcBeginPanningFeedback(HWND hwnd);

/**
 * Calls @c BufferedPaintClear() if available (and @c UXTHEME.DLL is in use),
 * orreturns @c E_NOTIMPL.
 *
 * @param hBufferedPaint
 * @param prc
 * @return Return value of @c BufferedPaintClear() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcBufferedPaintClear(HPAINTBUFFER hBufferedPaint,
            const RECT* prc);

/**
 * Calls @c BufferedPaintInit() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @return Return value of @c BufferedPaintInit() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcBufferedPaintInit(void);

/**
 * Calls @c BufferedPaintRenderAnimation() if available (and @c UXTHEME.DLL is
 * in use), or returns @c FALSE.
 *
 * @param hwnd
 * @param hdcTarget
 * @return Return value of @c mcBufferedPaintRenderAnimation() or @c FALSE.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
BOOL MCTRL_API mcBufferedPaintRenderAnimation(HWND hwnd, HDC hdcTarget);

/**
 * Calls @c BufferedPaintSetAlpha() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hBufferedPaint
 * @param prc
 * @param alpha
 * @return Return value of @c BufferedPaintSetAlpha() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcBufferedPaintSetAlpha(HPAINTBUFFER hBufferedPaint,
            const RECT* prc, BYTE alpha);

/**
 * Calls @c BufferedPaintStopAllAnimations() if available (and @c UXTHEME.DLL
 * is in use), or returns @c E_NOTIMPL.
 *
 * @param hwnd
 * @return Return value of @c BufferedPaintStopAllAnimations() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcBufferedPaintStopAllAnimations(HWND hwnd);

/**
 * Calls @c BufferedPaintUnInit() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @return Return value of @c BufferedPaintUnInit() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcBufferedPaintUnInit(void);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c CloseThemeData() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @return Return value of @c CloseThemeData() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcCloseThemeData(HTHEME hTheme);

/**
 * Calls @c DrawThemeBackground() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prc
 * @param prcClip
 * @return Return value of @c DrawThemeBackground() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcDrawThemeBackground(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prc, const RECT* prcClip);

/**
 * Calls @c DrawThemeBackgroundEx() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prc
 * @param pOptions
 * @return Return value of @c DrawThemeBackgroundEx() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcDrawThemeBackgroundEx(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prc, const DTBGOPTS* pOptions);

/**
 * Calls @c DrawThemeEdge() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prcDest
 * @param uEdge
 * @param uFlags
 * @param prcContent
 * @return Return value of @c DrawThemeEdge() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcDrawThemeEdge(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prcDest, UINT uEdge,
            UINT uFlags, RECT* prcContent);

/**
 * Calls @c DrawThemeIcon() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prc
 * @param himl
 * @param iImageIndex
 * @return Return value of @c DrawThemeIcon() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcDrawThemeIcon(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prc,
            HIMAGELIST himl, int iImageIndex);

/**
 * Calls @c DrawThemeParentBackground() if available (and @c UXTHEME.DLL is
 * in use).
 *
 * If it is not, the function fallbacks to asking the parent to paint itself
 * by sending @c WM_ERASEBKGND and @c WM_PRINTCLIENT.
 *
 * @param hwnd
 * @param hdc
 * @param prc
 * @return Return value of @c DrawThemeParentBackground() or @c S_OK.
 */
HRESULT MCTRL_API mcDrawThemeParentBackground(HWND hwnd, HDC hdc, RECT* prc);


#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c DrawThemeParentBackgroundEx() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hwnd
 * @param hdc
 * @param dwFlags
 * @param prc
 * @return Return value of @c DrawThemeParentBackgroundEx() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcDrawThemeParentBackgroundEx(HWND hwnd, HDC hdc,
            DWORD dwFlags, RECT* prc);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c DrawThemeText() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param pszText
 * @param iCharCount
 * @param dwFlags
 * @param dwFlags2
 * @param prc
 * @return Return value of @c DrawThemeText() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcDrawThemeText(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const WCHAR* pszText, int iCharCount,
            DWORD dwFlags, DWORD dwFlags2, const RECT* prc);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c DrawThemeTextEx() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param pszText
 * @param iCharCount
 * @param dwFlags
 * @param prc
 * @param pOptions
 * @return Return value of @c DrawThemeTextEx() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcDrawThemeTextEx(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const WCHAR* pszText, int iCharCount,
            DWORD dwFlags, RECT* prc, const DTTOPTS* pOptions);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c EnableThemeDialogTexture() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hwnd
 * @param dwFlags
 * @return Return value of @c EnableThemeDialogTexture() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcEnableThemeDialogTexture(HWND hwnd, DWORD dwFlags);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c EndBufferedAnimation() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hbpAnimation
 * @param fUpdateTarget
 * @return Return value of @c EndBufferedAnimation() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcEndBufferedAnimation(HANIMATIONBUFFER hbpAnimation,
            BOOL fUpdateTarget);

/**
 * Calls @c EndBufferedPaint() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hBufferedPaint
 * @param fUpdateTarget
 * @return Return value of @c EndBufferedPaint() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcEndBufferedPaint(HPAINTBUFFER hBufferedPaint,
            BOOL fUpdateTarget);

/**
 * Calls @c EndPanningFeedback() if available (and @c UXTHEME.DLL is in use),
 * or returns @c FALSE.
 *
 * @param hwnd
 * @param fAnimateBack
 * @return Return value of @c EndPanningFeedback() or @c FALSE.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
BOOL MCTRL_API mcEndPanningFeedback(HWND hwnd, BOOL fAnimateBack);

/**
 * Calls @c GetBufferedPaintBits() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hBufferedPaint
 * @param ppbBuffer
 * @param pcxRow
 * @return Return value of @c GetBufferedPaintBits() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcGetBufferedPaintBits(HPAINTBUFFER hBufferedPaint,
            RGBQUAD** ppbBuffer, int* pcxRow);

/**
 * Calls @c GetBufferedPaintDC() if available (and @c UXTHEME.DLL is in use),
 * or returns @c NULL.
 *
 * @param hBufferedPaint
 * @return Return value of @c GetBufferedPaintDC() or @c NULL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HDC MCTRL_API mcGetBufferedPaintDC(HPAINTBUFFER hBufferedPaint);

/**
 * Calls @c GetBufferedPaintTargetDC() if available (and @c UXTHEME.DLL is
 * in use), or returns @c NULL.
 *
 * @param hBufferedPaint
 * @return Return value of @c GetBufferedPaintTargetDC() or @c NULL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HDC MCTRL_API mcGetBufferedPaintTargetDC(HPAINTBUFFER hBufferedPaint);

/**
 * Calls @c GetBufferedPaintTargetRect() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hBufferedPaint
 * @param prc
 * @return Return value of @c GetBufferedPaintTargetRect() or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcGetBufferedPaintTargetRect(HPAINTBUFFER hBufferedPaint,
            RECT* prc);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c GetCurrentThemeName() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param pszThemeFilename
 * @param cchMaxFilenameChars
 * @param pszColorBuff
 * @param cchMaxColorChars
 * @param pszSizeBuff
 * @param cchMaxSizeChars
 * @return Return value of @c GetCurrentThemeName() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetCurrentThemeName(
            WCHAR* pszThemeFilename, int cchMaxFilenameChars,
            WCHAR* pszColorBuff, int cchMaxColorChars,
            WCHAR* pszSizeBuff, int cchMaxSizeChars);

/**
 * Calls @c GetThemeAppProperties() if available (and @c UXTHEME.DLL is in use),
 * or returns @c NULL.
 *
 * @return Return value of @c GetThemeAppProperties() or @c 0.
 */
DWORD MCTRL_API mcGetThemeAppProperties(void);

/**
 * Calls @c GetThemeBackgroundContentRect() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prcBounding
 * @param prcContent
 * @return Return value of @c GetThemeBackgroundContentRect() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prcBounding, RECT* prcContent);

/**
 * Calls @c GetThemeBackgroundExtent() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prcContent
 * @param prcExtent
 * @return Return value of @c GetThemeBackgroundExtent() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeBackgroundExtent(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prcContent, RECT* prcExtent);

/**
 * Calls @c GetThemeBackgroundRegion() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prc
 * @param phRegion
 * @return Return value of @c GetThemeBackgroundRegion() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeBackgroundRegion(HTHEME hTheme, HDC hdc,
            int iPartId, int iStateId, const RECT* prc, HRGN* phRegion);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c GetThemeBitmap() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param uFlags
 * @param phBitmap
 * @return Return value of @c GetThemeBitmap() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeBitmap(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, ULONG uFlags, HBITMAP* phBitmap);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c GetThemeBool() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pfValue
 * @return Return value of @c GetThemeBool() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeBool(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, BOOL* pfValue);

/**
 * Calls @c GetThemeColor() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pColor
 * @return Return value of @c GetThemeColor() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeColor(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, COLORREF* pColor);

/**
 * Calls @c GetThemeDocumentationProperty() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param pszThemeName
 * @param pszPropName
 * @param pszValueBuf
 * @param cchMaxValChars
 * @return Return value of @c GetThemeDocumentationProperty() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeDocumentationProperty(const WCHAR* pszThemeName,
            const WCHAR* pszPropName, WCHAR* pszValueBuf, int cchMaxValChars);

/**
 * Calls @c GetThemeEnumValue() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param piValue
 * @return Return value of @c GetThemeEnumValue() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeEnumValue(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, int* piValue);

/**
 * Calls @c GetThemeFilename() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pszThemeFilename
 * @param cchMaxBuffChars
 * @return Return value of @c GetThemeFilename() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeFilename(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, WCHAR* pszThemeFilename, int cchMaxBuffChars);

/**
 * Calls @c GetThemeFont() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pLogFont
 * @return Return value of @c GetThemeFont() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeFont(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            int iPropId, LOGFONTW* pLogFont);

/**
 * Calls @c GetThemeInt() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param piValue
 * @return Return value of @c GetThemeInt() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeInt(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, int* piValue);

/**
 * Calls @c GetThemeIntList() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pIntList
 * @return Return value of @c GetThemeIntList() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeIntList(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, INTLIST* pIntList);

/**
 * Calls @c GetThemeMargins() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param prc
 * @param pMargins
 * @return Return value of @c GetThemeMargins() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, int iPropId, RECT* prc, MARGINS* pMargins);

/**
 * Calls @c GetThemeMetric() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param piValue
 * @return Return value of @c GetThemeMetric() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeMetric(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, int iPropId, int* piValue);

/**
 * Calls @c GetThemePartSize() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param prc
 * @param eSize
 * @param psz
 * @return Return value of @c GetThemePartSize() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemePartSize(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, const RECT* prc, enum THEMESIZE eSize, SIZE* psz);

/**
 * Calls @c GetThemePosition() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pPoint
 * @return Return value of @c GetThemePosition() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemePosition(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, POINT* pPoint);

/**
 * Calls @c GetThemePropertyOrigin() if available (and @c UXTHEME.DLL is in
 * use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pOrigin
 * @return Return value of @c GetThemePropertyOrigin() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemePropertyOrigin(HTHEME hTheme, int iPartId,
            int iStateId, int iPropId, enum PROPERTYORIGIN* pOrigin);

/**
 * Calls @c GetThemeRect() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param prc
 * @return Return value of @c GetThemeRect() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeRect(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, RECT* prc);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c GetThemeStream() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param ppvStream
 * @param pcbStream
 * @param hInst
 * @return Return value of @c GetThemeStream() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeStream(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, void** ppvStream, DWORD* pcbStream, HINSTANCE hInst);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c GetThemeString() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @param iPropId
 * @param pszBuff
 * @param cchMaxBuffChars
 * @return Return value of @c GetThemeString() or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeString(HTHEME hTheme, int iPartId, int iStateId,
            int iPropId, WCHAR* pszBuff, int cchMaxBuffChars);

/**
 * Calls @c GetThemeSysBool() if available (and @c UXTHEME.DLL is in use).
 *
 * If it is not, mCtrl falls back to heuristics based on information from
 * @c SystemParametersInfo().
 *
 * @param hTheme
 * @param iBoolId
 * @return Return value of @c GetThemeSysBool(), or from the fallback
 * implementation.
 */
BOOL MCTRL_API mcGetThemeSysBool(HTHEME hTheme, int iBoolId);

/**
 * Calls @c GetThemeSysColor() if available (and @c UXTHEME.DLL is in use).
 * If it is not, mCtrl falls back to @c GetSysColor().
 *
 * @param hTheme
 * @param iColorId
 * @return Return value of @c GetThemeSysColor(), or from @c GetSysColor().
 */
COLORREF MCTRL_API mcGetThemeSysColor(HTHEME hTheme, int iColorId);

/**
 * Calls @c GetThemeSysColorBrush() if available (and @c UXTHEME.DLL is in use).
 * If it is not, mCtrl falls back to implementation based on @c GetSysColor().
 *
 * @param hTheme
 * @param iColorId
 * @return Return value of @c GetThemeSysColor(), or from the fallback
 * implementation.
 */
HBRUSH MCTRL_API mcGetThemeSysColorBrush(HTHEME hTheme, int iColorId);

/**
 * Calls @c GetThemeSysFont() if available (and @c UXTHEME.DLL is in use).
 * If it is not, mCtrl falls back to implementation based on
 * @c SystemParameterInfo().
 *
 * @param hTheme
 * @param iFontId
 * @param pLogFont
 * @return Return value of @c GetThemeSysFont(), or a @c HRESULT based on
 * success of the fallback implementation.
 */
HRESULT MCTRL_API mcGetThemeSysFont(HTHEME hTheme, int iFontId,
            LOGFONTW* pLogFont);

/**
 * Calls @c GetThemeSysInt() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iIntId
 * @param piValue
 * @return Return value of @c GetThemeSysInt(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeSysInt(HTHEME hTheme, int iIntId, int* piValue);

/**
 * Calls @c GetThemeSysSize() if available (and @c UXTHEME.DLL is in use), or
 * falls back to implementation based on @c GetSystemMetrics().
 *
 * @param hTheme
 * @param iSizeId
 * @return Return value of @c GetThemeSysSize(), or from the fallback
 * implementation.
 */
int MCTRL_API mcGetThemeSysSize(HTHEME hTheme, int iSizeId);

/**
 * Calls @c GetThemeSysString() if available (and @c UXTHEME.DLL is in use), or
 * returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param iStringId
 * @param pszBuff
 * @param cchMaxBuffChars
 * @return Return value of @c GetThemeSysSize(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeSysString(HTHEME hTheme, int iStringId,
            WCHAR* pszBuff, int cchMaxBuffChars);

/**
 * Calls @c GetThemeTextExtent() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param pszText
 * @param cchTextMax
 * @param dwFlags
 * @param prcBounding
 * @param prcExtent
 * @return Return value of @c GetThemeTextExtent(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeTextExtent(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, const WCHAR* pszText, int cchTextMax, DWORD dwFlags,
            const RECT* prcBounding, RECT* prcExtent);

/**
 * Calls @c GetThemeTextMetrics() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param pTextMetric
 * @return Return value of @c GetThemeTextMetrics(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcGetThemeTextMetrics(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, TEXTMETRIC* pTextMetric);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c GetThemeTransitionDuration() if available (and @c UXTHEME.DLL is in
 * use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param pTextMetric
 * @return Return value of @c GetThemeTextMetrics(), or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcGetThemeTransitionDuration(HTHEME hTheme, int iPartId,
            int iStateIdFrom, int iStateIdTo, int iPropId, DWORD* pdwDuration);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c GetWindowTheme() if available (and @c UXTHEME.DLL is in use), or
 * returns @c NULL.
 *
 * @param hwnd
 * @return Return value of @c GetWindowTheme(), or @c NULL.
 */
HTHEME MCTRL_API mcGetWindowTheme(HWND hwnd);

/**
 * Calls @c HitTestThemeBackground() if available (and @c UXTHEME.DLL is in
 * use), or returns @c E_NOTIMPL.
 *
 * @param hTheme
 * @param hdc
 * @param iPartId
 * @param iStateId
 * @param dwOptions
 * @param prc
 * @param hrgn
 * @param ptTest
 * @param pwHitTestCode
 * @return Return value of @c HitTestThemeBackground(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcHitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId,
            int iStateId, DWORD dwOptions, const RECT* prc, HRGN hrgn,
            POINT ptTest, WORD* pwHitTestCode);

/**
 * Calls @c IsAppThemed() if available (and @c UXTHEME.DLL is in use), or
 * returns @c FALSE.
 *
 * @return Return value of @c IsAppThemed(), or @c FALSE.
 */
BOOL MCTRL_API mcIsAppThemed(void);

#if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c IsCompositionActive() if available (and @c UXTHEME.DLL is in use),
 * or returns @c FALSE.
 *
 * @return Return value of @c IsCompositionActive(), or @c FALSE.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
BOOL MCTRL_API mcIsCompositionActive(void);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c IsThemeActive() if available (and @c UXTHEME.DLL is in use), or
 * returns @c FALSE.
 *
 * @return Return value of @c IsThemeActive(), or @c FALSE.
 */
BOOL MCTRL_API mcIsThemeActive(void);

/**
 * Calls @c IsThemeBackgroundPartiallyTransparent() if available (and
 * @c UXTHEME.DLL is in use), or returns @c FALSE.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @return Return value of @c IsThemeBackgroundPartiallyTransparent(), or
 * @c FALSE.
 */
BOOL MCTRL_API mcIsThemeBackgroundPartiallyTransparent(HTHEME hTheme,
            int iPartId, int iStateId);

/**
 * Calls @c IsThemeDialogTextureEnabled() if available (and @c UXTHEME.DLL is
 * in use), or returns @c FALSE.
 *
 * @param hwnd
 * @return Return value of @c IsThemeDialogTextureEnabled(), or @c FALSE.
 */
BOOL MCTRL_API mcIsThemeDialogTextureEnabled(HWND hwnd);

/**
 * Calls @c IsThemePartDefined() if available (and @c UXTHEME.DLL is in use),
 * or returns @c FALSE.
 *
 * @param hTheme
 * @param iPartId
 * @param iStateId
 * @return Return value of @c IsThemePartDefined(), or @c FALSE.
 */
BOOL MCTRL_API mcIsThemePartDefined(HTHEME hTheme, int iPartId, int iStateId);

/**
 * Calls @c OpenThemeData() if available (and @c UXTHEME.DLL is in use),
 * or returns @c MULL.
 *
 * @param hwnd
 * @param pszClassList
 * @return Return value of @c OpenThemeData(), or @c FALSE.
 */
HTHEME MCTRL_API mcOpenThemeData(HWND hwnd, const WCHAR* pszClassList);

/**
 * Calls @c OpenThemeDataEx() if available (and @c UXTHEME.DLL is in use),
 * or returns @c MULL.
 *
 * @param hwnd
 * @param pszClassList
 * @param dwFlags
 * @return Return value of @c OpenThemeDataEx(), or @c FALSE.
 */
HTHEME MCTRL_API mcOpenThemeDataEx(HWND hwnd, const WCHAR* pszClassList,
            DWORD dwFlags);

/**
 * Calls @c SetThemeAppProperties() if available (and @c UXTHEME.DLL is in use).
 * @param dwFlags
 */
void MCTRL_API mcSetThemeAppProperties(DWORD dwFlags);

/**
 * Calls @c SetWindowTheme() if available (and @c UXTHEME.DLL is in use),
 * or returns @c E_NOTIMPL.
 *
 * @param hwnd
 * @param pszSubAppName
 * @param pszSubIdList
 * @return Return value of @c SetWindowTheme(), or @c E_NOTIMPL.
 */
HRESULT MCTRL_API mcSetWindowTheme(HWND hwnd, const WCHAR* pszSubAppName,
            const WCHAR* pszSubIdList);

 #if (_WIN32_WINNT >= 0x0600)
/**
 * Calls @c SetWindowThemeAttribute() if available (and @c UXTHEME.DLL is
 * in use), or returns @c E_NOTIMPL.
 *
 * @param hwnd
 * @param eAttribute
 * @param pvAttribute
 * @param cbAttribute
 * @return Return value of @c SetWindowThemeAttribute(), or @c E_NOTIMPL.
 *
 * @note Requires @c _WIN32_WINNT to be @c 0x0600 or newer.
 */
HRESULT MCTRL_API mcSetWindowThemeAttribute(HWND hwnd,
            enum WINDOWTHEMEATTRIBUTETYPE eAttribute, void* pvAttribute,
            DWORD cbAttribute);
#endif  /* _WIN32_WINNT >= 0x0600 */

/**
 * Calls @c UpdatePanningFeedback() if available (and @c UXTHEME.DLL
 * is in use), or returns @c FALSE.
 *
 * @param hwnd
 * @param lTotalOverpanOffsetX
 * @param lTotalOverpanOffsetY
 * @param fInInertia
 * @return Return value of @c UpdatePanningFeedback(), or @c FALSE.
 */
BOOL MCTRL_API mcUpdatePanningFeedback(HWND hwnd, LONG lTotalOverpanOffsetX,
            LONG lTotalOverpanOffsetY, BOOL fInInertia);

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_THEME_H */
