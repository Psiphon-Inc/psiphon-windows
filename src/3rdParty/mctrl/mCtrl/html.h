/*
 * Copyright (c) 2008-2015 Martin Mitas
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
 * @section html_generated_contents Dynamically Generated Contents
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
 * @section html_script Calling Script Function
 *
 * The control supports also invoking a script (e.g. JavaScript) function
 * within the HTML page from the application's code.
 *
 * There are actually two messages for this very purpose. The message @ref
 * MC_HM_CALLSCRIPTFUNCEX is more powerful, and can call any script function,
 * with any number of arguments of any type, and returning any type, but using
 * this message requires manual setup of OLE variadic type (@c VARIANT) type
 * and it requires more coding.
 *
 * The other message, @ref MC_HM_CALLSCRIPTFUNC, is easier to use but its use
 * imposes some limitations: It can only deal with script functions with up
 * to four arguments, and all arguments, as well as any return value, must be
 * of string or integer type.
 *
 *
 * @section html_gotchas Gotchas
 *
 * - Keep in mind that the control is relatively thin wrapper of embedded MS
 *   Internet Explorer so exact behavior depends on the version of MS IE
 *   installed.
 *
 * - The value of the URL in notifications might not match the URL that was
 *   originally given to the HTML control, because the URL might be converted
 *   to a qualified form. For example, the IE sometimes may add a slash ('/')
 *   at the end of some URLs. Furthermore IE can encode some special characters
 *   into their hexadecimal representation (e.g. space ' ' becomes "%20").
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

/** @brief Indicates that control should ask main window before navigating */
#define MC_HS_NOTIFYNAV        0x0002

/*@}*/


/**
 * @name Message Structures
 */
/*@{*/

/**
 * @brief Structure for message @ref MC_HM_CALLSCRIPTFUNCW request (Unicode variant).
 * @sa MC_HM_CALLSCRIPTFUNCW
 */
typedef struct MC_HMCALLSCRIPTFUNCW_tag {
    /** Set to @c sizeof(MC_HMCALLSCRIPTFUNCW). */
    UINT cbSize;
    /** Set to address of a buffer to store string result of the function call,
     *  or to @c NULL if the expected return value is of integer type or
     *  there is no return value. */
    LPWSTR pszRet;
    /** If @c pszRet is not @c NULL, set to size of the buffer. If @c pszRet
     *  is @c NULL and the function returns integer, it is stored here. */
    int iRet;
    /** Set to number of arguments passed to the function. (Four at most.) */
    UINT cArgs;
    /** Specify 1st argument (if it is of string type). */
    LPCWSTR pszArg1;
    /** Specify 1st argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg1;
    /** Specify 2nd argument (if it is of string type). */
    LPCWSTR pszArg2;
    /** Specify 2nd argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg2;
    /** Specify 3rd argument (if it is of string type). */
    LPCWSTR pszArg3;
    /** Specify 3rd argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg3;
    /** Specify 4th argument (if it is of string type). */
    LPCWSTR pszArg4;
    /** Specify 4th argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg4;
} MC_HMCALLSCRIPTFUNCW;

/**
 * @brief Structure for message @ref MC_HM_CALLSCRIPTFUNCA request (ANSI variant).
 * @sa MC_HM_CALLSCRIPTFUNCA
 */
typedef struct MC_HMCALLSCRIPTFUNCA_tag {
    /** Set to @c sizeof(MC_HMCALLSCRIPTFUNCA). */
    UINT cbSize;
    /** Set to address of a buffer to store string result of the function call,
     *  or to @c NULL if the expected return value is of integer type or
     *  there is no return value. */
    LPCSTR pszRet;
    /** If @c pszRet is not @c NULL, set to size of the buffer. If @c pszRet
     *  is @c NULL and the function returns integer, it is stored here. */
    int iRet;
    /** Set to number of arguments passed to the function. (Four at most.) */
    UINT cArgs;
    /** Specify 1st argument (if it is of string type). */
    LPCSTR pszArg1;
    /** Specify 1st argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg1;
    /** Specify 2nd argument (if it is of string type). */
    LPCSTR pszArg2;
    /** Specify 2nd argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg2;
    /** Specify 3rd argument (if it is of string type). */
    LPCSTR pszArg3;
    /** Specify 3rd argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg3;
    /** Specify 4th argument (if it is of string type). */
    LPCSTR pszArg4;
    /** Specify 4th argument (if it is of integer type). Ignored if
     *  @c pszArg1 is not @c NULL. */
    int iArg4;
} MC_HMCALLSCRIPTFUNCA;

/**
 * @brief Structure for message @ref MC_HM_CALLSCRIPTFUNCEX.
 * @sa MC_HM_CALLSCRIPTFUNCEX
 */
typedef struct MC_HMCALLSCRIPTFUNCEX_tag {
    /** Set to @c sizeof(MC_HMCALLSCRIPTFUNCEX). */
    UINT cbSize;
    /** Name of function to call. */
    const LPCOLESTR pszFuncName;
    /** Pointer to array of arguments to be passed to the function. */
    VARIANT* lpvArgs;
    /** Count of the arguments. */
    UINT cArgs;
    /** Pointer to @c VARIANT which receives the return value.
     *  May be @c NULL if caller does not expect to get a return value
     *  (or if the caller ignores it).
     *  If not @c NULL, the caller should initialize it to @c VT_EMPTY
     *  before making the call and, after it returns, the caller is responsible
     *  for its contents. I.e. if the returned type is @c VT_BSTR, the caller
     *  must eventually free the string with @c SysFreeString(). */
    VARIANT* lpRet;
} MC_HMCALLSCRIPTFUNCEX;

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

/**
 * @brief Calls script function in HTML page (Unicode variant).
 * @param[in] wParam (@c LPCWSTR*) Name of the function to call.
 * @param[in,out] lParam (@ref MC_HMCALLSCRIPTFUNCW*) Pointer to a function
 * specifying function arguments and receiving the return value. May be @c NULL
 * if the function takes no arguments and returns no value (or the return value
 * is ignored).
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_CALLSCRIPTFUNCW    (MC_HM_FIRST + 16)

/**
 * @brief Calls script function in HTML page (ANSI variant).
 * @param[in] wParam (@c LPCSTR*) Name of the function to call.
 * @param[in,out] lParam (@ref MC_HMCALLSCRIPTFUNCA*) Pointer to a function
 * specifying function arguments and receiving the return value. May be @c NULL
 * if the function takes no arguments and returns no value (or the return value
 * is ignored).
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_CALLSCRIPTFUNCA    (MC_HM_FIRST + 17)

/**
 * @brief Call script function in HTML page.
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_HMCALLSCRIPTFUNCEX*) Pointer to structure
 * specifying function to call, arguments to pass, and receiving the return
 * value.
 * @return (@c HRESULT) @c S_OK if the call was invokes successfully,
 * otherwise the @c HRESULT code of the error.
 */
#define MC_HM_CALLSCRIPTFUNCEX   (MC_HM_FIRST + 18)

/*@}*/


/**
 * @name Notifications Structures
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

 /**
 * @brief Fired before the browser navigates to a new URL. 
 *
 * Note that this is sent before (@ref MC_HN_APPLINK), and returning non-zero 
 * will prevent (@ref MC_HN_APPLINK) from being sent.
 *  
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL. 
 * @return Application should return zero if navigation should continue.
 */
#define MC_HN_BEFORENAVIGATE     (MC_HN_FIRST + 8)

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
/** Unicode-resolution alias. @sa MC_HM_CALLSCRIPTFNW MC_HM_CALLSCRIPTFNA */
#define MC_HM_CALLSCRIPTFUNC   MCTRL_NAME_AW(MC_HM_CALLSCRIPTFUNC)
/** Unicode-resolution alias. @sa MC_NMHTMLURLW MC_NMHTMLURLA */
#define MC_NMHTMLURL           MCTRL_NAME_AW(MC_NMHTMLURL)
/** Unicode-resolution alias. @sa MC_NMHTMLTEXTW MC_NMHTMLTEXTA */
#define MC_NMHTMLTEXT          MCTRL_NAME_AW(MC_NMHTMLTEXT)
/** Unicode-resolution alias. @sa MC_NMHTTPERRORW MC_NMHTTPERRORA */
#define MC_NMHTTPERROR         MCTRL_NAME_AW(MC_NMHTTPERROR)
/** Unicode-resolution alias. @sa MC_HMCALLSCRIPTFUNCW MC_HMCALLSCRIPTFUNCA */
#define MC_HMCALLSCRIPTFUNC   MCTRL_NAME_AW(MC_HMCALLSCRIPTFUNC)

/*@}*/



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_HTML_H */
