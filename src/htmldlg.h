/*
 * Copyright (c) 2012, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/*
 Returns -1 on error, 0 if dialog was cancelled (or dialog return was a 
 non-string), 1 if dialog returns a string. Returned string will be put into
 o_result. args will be passed to dialog if non-null. 

 Magic JavaScript values and functions:
    
    window.dialogArguments
        Contains the value passed in args. Will be an empty string if args is NULL.

    window.returnValue
        Will be retrieved by this function. If it's a string, that value will
        be put into o_result and this function will return 1 (so, assumes OK
        or the like was clicked to close the dialog). If it's not a string, 
        this function will return 0 (so, assumes CANCEL or the like was clicked
        to close the dialog).

    window.close()
        Call this to close the dialog.        
*/

int ShowHTMLDlg(
        HWND hParentWnd, 
        LPCTSTR resourceName, 
        LPCTSTR urlFragment,
        LPCTSTR args,
        tstring& o_result);

// Returns a URL that can be used in a HTML control to retrieve the given resource.
tstring ResourceToUrl(LPCTSTR resourceName, LPCTSTR urlQuery, LPCTSTR urlFragment);
