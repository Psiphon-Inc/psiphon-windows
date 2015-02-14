/*
 * Copyright (c) 2008-2012 Martin Mitas
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

#ifndef MCTRL_HTML_H
#define MCTRL_HTML_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief HTML control (@c MC_WC_HTML).
 *
 * As the control name suggests, the control is intended to display HTML
 * documents. Actually the control is thin wrapper of Internet Explorer
 * COM object, so it can do much more: display a plethora of multimedia files,
 * take use of JavaScript etc.
 *
 * The easiest way how to show some document is to specify URL of target
 * document as control's window name. For example when created with
 * @c CreateWindow(), use the 2nd argument as the URL. This allows easy use
 * of the control in dialog templates.
 *
 * URL can also be set anytime later with message @ref MC_HM_GOTOURL.
 *
 * The control accepts any protocol understood by the Internet Explorer,
 * for example:
 * - http://www.example.org
 * - file://C:/page.html
 * - res://some_dll/id_of_resource
 * - its:helpFile.chm::page.htm
 *
 *
 * @section html_proto_res Resource Protocol
 *
 * The @c res: protocol is especially useful. It allows you to embed some
 * resources like HTML pages, cascading style sheets (CSS), images (PNG, JPG
 * etc.) or JavaScript files into binary of your application or any DLL it
 * uses.
 *
 * You can link to such resources with URL in format
 * "res://modname/res_type/res_id" where @c modname is name of the binary
 * module (usually filename of your program or any DLL it loads; or a full path
 * to other .EXE or .DLL file), @c res_type is type of the resource and @c
 * res_id is ID of the resource in the resource script (RC).
 *
 * The type can be omitted in the URL. The control then assumes type 23
 * (@c RT_HTML).
 *
 * Although @c res_id can be both string or number identifier, we recommend
 * to prefer string identifiers which end with dummy &quot;file extension&quot;,
 * hence making a hint to the browser about the image type. Without this the
 * browser tries to guess what type of the data the resource is, and that may
 * be unreliable. Remember the heuristics also differ in various versions of
 * MSIE.
 *
 * For example if you have a HTML file named @c some_page.html and an image
 * file @c image.png which can be linked from the HTML page, add the following
 * lines into your resource script:
 * @code
 * some_page.html HTML path/to/some_page.html
 * image.png HTML path/to/image.png
 * @endcode
 * which is used to build a @c MYLIBRARY.DLL used by your application then
 * your application can simply send the message @ref MC_HM_GOTOURL with URL
 * @c "res://mylibrary.dll/some_page.html".
 *
 * Of course, HTML documents stored in the resources then can also use
 * relative URLs to link to other documents and resources in the same
 * module (@c .EXE or @c .DLL).
 *
 *
 * @section html_proto_app Application Protocol
 *
 * @c MCTRL.DLL implements a simple application protocol @c app: which is
 * intended for integration of HTML contents into your application logic.
 *
 * Whenever user clicks on a link with URL starting with the @c "app:"
 * the control sends notification @ref MC_HN_APPLINK to its parent window
 * which is supposed to react programmatically. The control itself does not
 * interpret application link URLs in any way.
 *
 *
 * @section html_generated_contents Generated Contents
 *
 * Generating HTML contents programmatically is also possible to some degree.
 * Note however that the application is not supposed to generate whole
 * documents but only smaller snippets of them.
 *
 * The application can set contents of almost any tag (identified by HTML
 * attribute @c "id") with any custom string using the message @ref MC_HM_SETTAGCONTENTS.
 * The message takes the ID and the string as its arguments. Then, if the
 * currently loaded page has a tag with the given ID, the content of the
 * tag is replaced  and set to the given string. Any previous content of that
 * tag is removed. Remember the string has to follow HTML syntax and keep
 * integrity of the HTML document.
 *
 * Note the application should use the message @ref MC_HM_SETTAGCONTENTS
 * only after the HTML document intended to be changed is completely loaded,
 * i.e. anytime after the notification @ref MC_HN_DOCUMENTCOMPLETE is fired.
 *
 * @attention
 * Please note that for limitations of Internet Explorer, contents of these
 * tags can <b>not</b> be modified:<br>
 * @c COL, @c COLGROUP, @c FRAMESET, @c HEAD, @c HTML, @c STYLE, @c TABLE,
 * @c TBODY, @c TFOOT, @c THEAD, @c TITLE, @c TR.
 *
 * We recommend to use tags @c DIV or @c SPAN for the dynamic contents
 * injected by application code into the HTML pages.
 *
 *
 * @section html_gotchas Gotchas
 *
 * - Keep in mind that the control is relatively thin wrapper of embedded MS
 *   Internet Explorer so exact behavior depends on the version of MS IE
 *   installed.
 *
 * - The HTML control implementation is based on OLE and COM technologies.
 *   During creation of the control @c OleInitialize() is called. I.e.
 *   the OLE subsystem is initialized for every thread which creates
 *   the HTML control. @c OleUninitialize() is similarly called when the
 *   control is destroyed.
 *
 * - The value of the URL in notifications might not match the URL that was
 *   originally given to the HTML control, because the URL might be converted
 *   to a qualified form. For example, the IE sometimes adds slash ('/') after
 *   URLs having two slashes after protocol and of there is no slash after.
 *   Furthermore IE can encode some special characters into their hexadecimal
 *   representation (i.e. space ' ' becomes "%20").
 *
 *
 * @section std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c CCM_GETUNICODEFORMAT
 * - @c CCM_SETNOTIFYWINDOW
 * - @c CCM_SETUNICODEFORMAT
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
BOOL MCTRL_API mcHtml_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcHtml_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_HTMLW            L"mCtrl.html"
/** Window class name (ANSI variant). */
#define MC_WC_HTMLA             "mCtrl.html"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Disables context menu */
#define MC_HS_NOCONTEXTMENU    0x0001

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Displays a document specified by the given URL (Unicode variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) The URL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_GOTOURLW        (MC_HM_FIRST + 10)

/**
 * @brief Displays a document specified by the given URL (ANSI variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) The URL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_GOTOURLA        (MC_HM_FIRST + 11)

/**
 * @brief Set contents of the HTML tag with given attribute @c "id" (Unicode variant).
 * @param[in] wParam (@c const @c WCHAR*) ID of the tag.
 * @param[in] lParam (@c const @c WCHAR*) New contents of the tag.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 * @see @ref html_generated_contents
 */
#define MC_HM_SETTAGCONTENTSW (MC_HM_FIRST + 12)

/**
 * @brief Set contents of the HTML tag with given attribute @c "id" (ANSI variant).
 * @param[in] wParam (@c const @c char*) ID of the tag.
 * @param[in] lParam (@c const @c char*) New contents of the tag.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 * @see @ref html_generated_contents
 */
#define MC_HM_SETTAGCONTENTSA (MC_HM_FIRST + 13)

/**
 * @brief Navigates the HTML control back or forward in history.
 * @param[in] wParam (@c BOOL) Set to @c TRUE to go back in history or
 * @c FALSE to go forward.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 * @see MC_HM_CANBACK
 */
#define MC_HM_GOBACK          (MC_HM_FIRST + 14)

/**
 * @brief Tests whether going back or forward in history is possible.
 * @param[in] wParam (@c BOOL) Set to @c TRUE to test going back in history or
 * @c FALSE to going forward.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE if can go back or forward respectively, @c FALSE otherwise.
 * @see MC_HM_GOBACK
 */
#define MC_HM_CANBACK         (MC_HM_FIRST + 15)

/*PSIPHON*/
/**
* @brief Calls script in HTML page, passing optional arguments, returning optional result (Unicode variant).
* @param[in] wParam (@c MC_HMCALLSCRIPTFN) Pointer to structure containing function name, 
*   arguments to function, etc. @sa MC_HMCALLSCRIPTFN
* @param[out] lParam String buffer to receive result of function. Optional; may be NULL.
* @return (@c int) @c 0 if successful, @c -1 for generic error, other values for specific 
*   errors (like @c ERROR_INSUFFICIENT_BUFFER).
*/
#define MC_HM_CALLSCRIPTFNW   (MC_HM_FIRST + 16)

/**
* @brief Calls script in HTML page, passing optional arguments, returning optional result (ANSI variant).
* @param[in] wParam (@c MC_HMCALLSCRIPTFN) Pointer to structure containing function name,
*   arguments to function, etc. @sa MC_HMCALLSCRIPTFN
* @param[out] lParam String buffer to receive result of function. Optional; may be NULL.
* @return (@c int) @c 0 if successful, @c -1 for generic error, other values for specific
*   errors (like @c ERROR_INSUFFICIENT_BUFFER).
*/
#define MC_HM_CALLSCRIPTFNA   (MC_HM_FIRST + 17)
/*/PSIPHON*/

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure used for notifications with URL parameter (Unicode variant).
 * @sa MC_HN_APPLINK MC_HN_DOCUMENTCOMPLETE
 */
typedef struct MC_NMHTMLURLW_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** String representation of the URL. */
    LPCWSTR pszUrl;
} MC_NMHTMLURLW;

/**
 * @brief Structure used for notifications with URL parameter (ANSI variant).
 * @sa MC_HN_APPLINK MC_HN_DOCUMENTCOMPLETE
 */
typedef struct MC_NMHTMLURLA_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** String representation of the URL. */
    LPCSTR pszUrl;
} MC_NMHTMLURLA;

/**
 * @brief Structure used for notification about download progress.
 * @sa MC_HN_PROGRESS
 */
typedef struct MC_NMHTMLPROGRESS_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** Current progress. */
    LONG lProgress;
    /** Progress maximum. */
    LONG lProgressMax;
} MC_NMHTMLPROGRESS;

/**
 * @brief Structure used for notifications with textual parameter (Unicode variant).
 * @sa MC_HN_STATUSTEXT MC_HN_TITLETEXT
 */
typedef struct MC_NMHTMLTEXTW_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** The string. */
    LPCWSTR pszText;
} MC_NMHTMLTEXTW;

/**
 * @brief Structure used for notifications with textual parameter (ANSI variant).
 * @sa MC_HN_STATUSTEXT MC_HN_TITLETEXT
 */
typedef struct MC_NMHTMLTEXTA_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** The string. */
    LPCSTR pszText;
} MC_NMHTMLTEXTA;

/**
 * @brief Structure used for notification about history navigation.
 * @sa MC_HN_HISTORY
 */
typedef struct MC_NMHTMLHISTORY_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** @c TRUE if going back in history is possible. */
    BOOL bCanBack;
    /** @c TRUE if going forward in history is possible. */
    BOOL bCanForward;
} MC_NMHTMLHISTORY;

/**
 * @brief Structure used for notification about HTTP error (Unicode variant).
 * @sa MC_HN_HTTPERROR
 */
typedef struct MC_NMHTTPERRORW_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** String representation of the URL. */
    LPCWSTR pszUrl;
    /** HTTP status code. */
    int iStatus;
} MC_NMHTTPERRORW;

/**
 * @brief Structure used for notification about HTTP error (ANSI variant).
 * @sa MC_HN_HTTPERROR
 */
typedef struct MC_NMHTTPERRORA_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** String representation of the URL. */
    LPCSTR pszUrl;
    /** HTTP status code. */
    int iStatus;
} MC_NMHTTPERRORA;

/*PSIPHON*/
/**
* @brief Structure used to pass arguments in @c MC_HM_CALLSCRIPTFN request,
* and to receive the result (Unicode variant).
* @sa MC_HM_CALLSCRIPTFN
*/
typedef struct MC_HMCALLSCRIPTFNW_tag {
    /** The name of the function to call. */
    LPCWSTR pszFnName;
    /** The argument (e.g. JSON encoded) to pass to the function. 
        Optional. */
    LPCWSTR pszArguments;
    /** The size of the result buffer -- in characters passed as LPARAM. 
        Can be zero if no result needed. */
    UINT iResultBufCharCount;
} MC_HMCALLSCRIPTFNW;

/**
* @brief Structure used to pass arguments in @c MC_HM_CALLSCRIPTFN request,
* and to receive the result (ANSI variant).
* @sa MC_HM_CALLSCRIPTFN
*/
typedef struct MC_HMCALLSCRIPTFNA_tag {
    /** The name of the function to call. */
    LPCSTR pszFnName;
    /** The argument (e.g. JSON encoded) to pass to the function.
    Optional. */
    LPCSTR pszArguments;
    /** The size of the result buffer -- in characters passed as LPARAM.
    Can be zero if no result needed. */
    UINT iResultBufCharCount;
} MC_HMCALLSCRIPTFNA;
/*/PSIPHON*/

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when the browser should navigates to to URL with application protocol.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_APPLINK            (MC_HN_FIRST + 0)

/**
 * @brief Fired when loading of a document is complete.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_DOCUMENTCOMPLETE   (MC_HN_FIRST + 1)

/**
 * @brief Fired to inform application about download progress.
 *
 * This allows example for example to show a progress indicator.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLPROGRESS*) Pointer to a structure specifying
 * details about the progress.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_PROGRESS           (MC_HN_FIRST + 2)

/**
 * @brief Fired when the browser would like to change status text.
 *
 * IE usually shows this text in its status bar.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLTEXT*) Pointer to a structure specifying
 * the text.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_STATUSTEXT         (MC_HN_FIRST + 3)

/**
 * @brief Fired when the browser changes title of the HTML page.
 *
 * IE usually shows this in window caption or tab label.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLTEXT*) Pointer to a structure specifying
 * the text.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_TITLETEXT          (MC_HN_FIRST + 4)

/**
 * @brief Fired when possibility of going back or forward in history changes.
 *
 * This allow application to enable or disable the corresponding command
 * buttons.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * the text.
 * @return Application should return zero if it processes the notification.
 *
 * @sa MC_HM_GOBACK MC_HM_CANBACK
 */
#define MC_HN_HISTORY            (MC_HN_FIRST + 5)

/**
 * @brief Fired when the browser would open a new window.
 *
 * This happens for example if user clicks on a link while holding @c SHIFT.
 *
 * @c MC_NMHTMLURL::pszUrl is URL to be opened in the new window. Note however
 * that prior to Windows XP SP2, the URL is not filled.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL.
 * @return Application should return non-zero to allow opening the new window,
 * or zero to deny it.
 */
#define MC_HN_NEWWINDOW          (MC_HN_FIRST + 6)

/**
 * @brief Fired to indicate that HTTP error has occurred.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTTPERROR*) Pointer to a structure specifying
 * details about the error.
 * @return Application should return zero to allow browser to show standard
 * error page corresponding to the error, or zero to disable that.
 */
#define MC_HN_HTTPERROR          (MC_HN_FIRST + 7)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_HTMLW MC_WC_HTMLA */
#define MC_WC_HTML             MCTRL_NAME_AW(MC_WC_HTML)
/** Unicode-resolution alias. @sa MC_HM_GOTOURLW MC_HM_GOTOURLA */
#define MC_HM_GOTOURL          MCTRL_NAME_AW(MC_HM_GOTOURL)
/** Unicode-resolution alias. @sa MC_HM_SETTAGCONTENTSW MC_HM_SETTAGCONTENTSA */
#define MC_HM_SETTAGCONTENTS   MCTRL_NAME_AW(MC_HM_SETTAGCONTENTS)
/** Unicode-resolution alias. @sa MC_NMHTMLURLW MC_NMHTMLURLA */
#define MC_NMHTMLURL           MCTRL_NAME_AW(MC_NMHTMLURL)
/** Unicode-resolution alias. @sa MC_NMHTMLTEXTW MC_NMHTMLTEXTA */
#define MC_NMHTMLTEXT          MCTRL_NAME_AW(MC_NMHTMLTEXT)
/** Unicode-resolution alias. @sa MC_NMHTTPERRORW MC_NMHTTPERRORA */
#define MC_NMHTTPERROR         MCTRL_NAME_AW(MC_NMHTTPERROR)

/*PSIPHON*/
/** Unicode-resolution alias. @sa MC_HM_CALLSCRIPTFNW MC_HM_CALLSCRIPTFNA */
#define MC_HM_CALLSCRIPTFN     MCTRL_NAME_AW(MC_HM_CALLSCRIPTFN)
/** Unicode-resolution alias. @sa MC_HMCALLSCRIPTFNW MC_HMCALLSCRIPTFNA */
#define MC_HMCALLSCRIPTFN      MCTRL_NAME_AW(MC_HMCALLSCRIPTFN)
/*/PSIPHON*/

/*@}*/



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_HTML_H */
