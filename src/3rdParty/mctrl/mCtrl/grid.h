/*
 * Copyright (c) 2010-2014 Martin Mitas
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

#ifndef MCTRL_GRID_H
#define MCTRL_GRID_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>
#include <mCtrl/value.h>
#include <mCtrl/table.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Grid control (@c MC_WC_GRID).
 *
 * The grid control provides user interface for presentation of a lot of data
 * in, as the name of the control suggests, a grid.
 *
 *
 * @section grid_model Data Model
 *
 * By default, the control uses @ref MC_HTABLE data model to manage the data
 * displayed by the control. Then, actually, all messages manipulating with
 * data hold by the control just call corresponding function manipulating with
 * the underlying @ref MC_HTABLE.
 *
 * By default, the control creates an empty table during its creation, of size
 * 0 x 0. So, usually, one of 1st messages sent to the control by any application
 * is @ref MC_GM_RESIZE.
 *
 * Alternatively, you can use the style @ref MC_GS_NOTABLECREATE. In that case
 * the control does not create any table during its creation, and you must
 * associate an existing table with the control with the message
 * @ref MC_GM_SETTABLE. Until you do so, all messages attempting
 * to modify the underlying table will just fail.
 *
 * @ref MC_GM_SETTABLE together with @ref MC_GM_GETTABLE allows attaching one
 * table to multiple controls.
 *
 * Messages which do not manipulate with the table determine how the table
 * is presented and these are tied to the control. I.e. if any table is
 * attached to multiple control, each of the controls can present the table
 * in other way (e.g. have another dimensions for each cell etc.).
 *
 *
 * @section grid_virtual Virtual Grid
 *
 * The grid can defer management of the data to the parent window instead of
 * using the @ref MC_HTABLE. To do this, application has to apply the style
 * @ref MC_GS_OWNERDATA.
 *
 * The control then uninstalls any table currently associated with it (if any),
 * and whenever painting a cell, it asks its parent for the data it needs.
 * The control uses the notification @ref MC_GN_GETDISPINFO to retrieve the data.
 *
 * The virtual grid is especially useful to present data calculated on the fly
 * or retrieved from some large database, so that only the required amount
 * of the data is calculated and retrieved, which can result in saving a lot of
 * memory and CPU cycles.
 *
 * The control actually only remembers dimensions of the virtual table as set
 * by @ref MC_GM_RESIZE.
 *
 * If the retrieval of the data is expensive operation, the application may
 * want to implement some caching scheme. For this purpose the control sends
 * the notification @ref MC_GN_ODCACHEHINT, which informs the application about
 * the region of cells it may ask most frequently. Note however the control is
 * free to ask for any cell, not just the one included in the range as specified
 * by the latest @ref MC_GN_ODCACHEHINT.
 *
 * Also that (if the control has the styles @ref MC_GS_COLUMNHEADERNORMAL
 * and/or @ref MC_GS_ROWHEADERNORMAL) also may frequently for the header
 * cells, although the headers are never included in @ref MC_GN_ODCACHEHINT.
 * The application should always cache the data for the column and row headers.
 *
 * Please remember that when the style @ref MC_GS_OWNERDATA is used, some
 * control messages and styles behave differently:
 *
 * - Message @ref MC_GM_RESIZE does not resize a table associated with the control
 *   but only informs the control how large the virtual table is (and application
 *   then has to be ready to provide data for any cell within the dimensions).
 *
 * - Messages @ref MC_GM_SETCELL, @ref MC_GM_GETCELL, @ref MC_GM_CLEAR,
 *   @ref MC_GM_SETTABLE do nothing: They all just fail (they just return
 *   @c FALSE).
 *
 * - Style @ref MC_GS_NOTABLECREATE has no effect (the @ref MC_GS_OWNERDATA
 *   actually does what @ref MC_GS_NOTABLECREATE and much more).
 *
 * To force repainting of one or more items, when the underlying data change,
 * the application is supposed to use the message @ref MC_GM_REDRAWCELLS.
 *
 *
 * @section std_msgs Standard Messages
 *
 * These standard messages are handled by @c MC_WC_GRID control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_GETUNICODEFORMAT
 * - @c CCM_SETNOTIFYWINDOW
 * - @c CCM_SETUNICODEFORMAT
 * - @c CCM_SETWINDOWTHEME
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcGrid_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcGrid_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_GRIDW            L"mCtrl.grid"
/** Window class name (ANSI variant). */
#define MC_WC_GRIDA             "mCtrl.grid"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Do not automatically create empty table. */
#define MC_GS_NOTABLECREATE          0x0001

/** @brief Do not paint grid lines. */
#define MC_GS_NOGRIDLINES            0x0002

/** @brief Use double buffering. */
#define MC_GS_DOUBLEBUFFER           0x0004

/** @brief Enable the virtual grid mode.
 *  @details See @ref grid_virtual for more info. */
#define MC_GS_OWNERDATA              0x0008

/* TODO:
#define MC_GS_RESIZABLECOLUMNS       0x0010
#define MC_GS_RESIZABLEROWS          0x0020
#define MC_GS_RESIZABLEHEADERS       0x0040
*/

/** @brief The contents of column headers is used. This is default. */
#define MC_GS_COLUMNHEADERNORMAL     0x0000
/** @brief Columns have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_COLUMNHEADERNUMBERED   0x1000
/** @brief Columns have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_COLUMNHEADERALPHABETIC 0x2000
/** @brief Columns have no header. */
#define MC_GS_COLUMNHEADERNONE       0x3000
/** @brief Bit mask specifying the column header mode. */
#define MC_GS_COLUMNHEADERMASK      (MC_GS_COLUMNHEADERNORMAL |               \
                                     MC_GS_COLUMNHEADERNUMBERED |             \
                                     MC_GS_COLUMNHEADERALPHABETIC |           \
                                     MC_GS_COLUMNHEADERNONE)

/** @brief The contents of row headers is used. This is default. */
#define MC_GS_ROWHEADERNORMAL        0x0000
/** @brief Rows have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_ROWHEADERNUMBERED      0x4000
/** @brief Rows have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_ROWHEADERALPHABETIC    0x8000
/** @brief Rows have no header. This is default. */
#define MC_GS_ROWHEADERNONE          0xC000
/** @brief Bit mask specifying the row header mode. */
#define MC_GS_ROWHEADERMASK         (MC_GS_ROWHEADERNORMAL |                  \
                                     MC_GS_ROWHEADERNUMBERED |                \
                                     MC_GS_ROWHEADERALPHABETIC |              \
                                     MC_GS_ROWHEADERNONE)

/*@}*/


/**
 * @name MC_GGEOMETRY::fMask Bits
 * @anchor MC_GGF_xxxx
 */
/*@{*/

/** @brief Set if @ref MC_GGEOMETRY::wColumnHeaderHeight is valid. */
#define MC_GGF_COLUMNHEADERHEIGHT     (1 << 0)
/** @brief Set if @ref MC_GGEOMETRY::wRowHeaderWidth is valid. */
#define MC_GGF_ROWHEADERWIDTH         (1 << 1)
/** @brief Set if @ref MC_GGEOMETRY::wDefColumnWidth is valid. */
#define MC_GGF_DEFCOLUMNWIDTH         (1 << 2)
/** @brief Set if @ref MC_GGEOMETRY::wDefRowHeight is valid. */
#define MC_GGF_DEFROWHEIGHT           (1 << 3)
/** @brief Set if @ref MC_GGEOMETRY::wPaddingHorz is valid. */
#define MC_GGF_PADDINGHORZ            (1 << 4)
/** @brief Set if @ref MC_GGEOMETRY::wPaddingVert is valid. */
#define MC_GGF_PADDINGVERT            (1 << 5)

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure describing inner geometry of the grid.
 * @sa MC_GM_SETGEOMETRY MC_GM_GETGEOMETRY
 */
typedef struct MC_GGEOMETRY_tag {
    /** Bitmask specifying what other members are valid. See @ref MC_GGF_xxxx. */
    DWORD fMask;
    /** Height of column header cells. */
    WORD wColumnHeaderHeight;
    /** Width of row header cells. */
    WORD wRowHeaderWidth;
    /** Default width of regular contents cells. */
    WORD wDefColumnWidth;
    /** Default height of regular contents cells. */
    WORD wDefRowHeight;
    /** Horizontal padding in cells. */
    WORD wPaddingHorz;
    /** Vertical padding in cells. */
    WORD wPaddingVert;
} MC_GGEOMETRY;

/**
 * @brief Structure used by notification @ref MC_GN_ODCACHEHINT.
 */
typedef struct MC_NMGCACHEHINT_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** First column of the region to be cached. */
    WORD wColumnFrom;
    /** First row of the region to be cached. */
    WORD wRowFrom;
    /** Last column of the region to be cached. */
    WORD wColumnTo;
    /** Last row of the region to be cached. */
    WORD wRowTo;
} MC_NMGCACHEHINT;

/**
 * @brief Structure used by notifications @ref MC_GN_GETDISPINFO and
 * @ref MC_GN_SETDISPINFO (Unicode variant).
 */
typedef struct MC_NMGDISPINFOW_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column index. */
    WORD wColumn;
    /** Row index. */
    WORD wRow;
    /** Structure describing the contents of the cell. */
    MC_TABLECELLW cell;
} MC_NMGDISPINFOW;

/**
 * @brief Structure used by notifications @ref MC_GN_GETDISPINFO and
 * @ref MC_GN_SETDISPINFO (ANSI variant).
 */
typedef struct MC_NMGDISPINFOA_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column index. */
    WORD wColumn;
    /** Row index. */
    WORD wRow;
    /** Structure describing the contents of the cell. */
    MC_TABLECELLA cell;
} MC_NMGDISPINFOA;

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Gets handle of table attached to the control or @c NULL if none is
 * attached.
 *
 * Note that calling the message does not change reference counter of the
 * returned table. If you want to preserve the handle, you should call
 * @ref mcTable_AddRef() on it and then @ref mcTable_Release() when you no
 * longer need it.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@ref MC_HTABLE) Handle of the table, or @c NULL.
 */
#define MC_GM_GETTABLE            (MC_GM_FIRST + 0)

/**
 * @brief Attaches a table to the control.
 *
 * Reference counter of the table is incremented. Previously attached table
 * (if any) is detached and its reference counter is decremented.
 *
 * If the @c wParam is @c NULL, the control creates new table (with reference
 * count set to one), unless the control has style @ref MC_GS_NOTABLECREATE.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_HTABLE) Handle of the table, or @c NULL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETTABLE            (MC_GM_FIRST + 1)

/**
 * @brief Gets count of columns in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table columns.
 */
#define MC_GM_GETCOLUMNCOUNT      (MC_GM_FIRST + 2)

/**
 * @brief Gets count of rows in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table rows.
 */
#define MC_GM_GETROWCOUNT         (MC_GM_FIRST + 3)

/**
 * @brief Resizes table attached to the control.
 *
 * @param[in] wParam (@c DWORD) The low-order word specifies count of columns,
 * high-order word specifies count of rows.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_RESIZE              (MC_GM_FIRST + 4)

/**
 * @brief Clears the table.
 *
 * @param[in] wParam Specification of the cells to be cleared. When set to zero,
 * all table contents (including header cells) is cleared. When non-zero, the
 * value is interpreted as a bit-mask of cells to clear:
 * Set bit @c 0x1 to clear all ordinary cells, @c 0x2 to clear column
 * headers and bit @c 0x4 to clear row cells.
 * @param lParam Reserved, set to zero.
 * @return Not defined, do not rely on return value.
 */
#define MC_GM_CLEAR               (MC_GM_FIRST + 5)

/**
 * @brief Sets a table cell (Unicode variant).
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[in] lParam (@ref MC_TABLECELLW*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELLW            (MC_GM_FIRST + 6)

/**
 * @brief Sets a table cell (ANSI variant).
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[in] lParam (@ref MC_TABLECELLA*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELLA            (MC_GM_FIRST + 7)

/**
 * @brief Gets a table cell (Unicode variant).
 *
 * Before calling this function, the member @c MC_TABLECELL::fMask must specify
 * what attributes of the cell to retrieve.
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[out] lParam (@ref MC_TABLECELLW*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETCELLW            (MC_GM_FIRST + 8)

/**
 * @brief Gets a table cell (ANSI variant).
 *
 * Before calling this function, the member @c MC_TABLECELL::fMask must specify
 * what attributes of the cell to retrieve.
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[out] lParam (@ref MC_TABLECELLA*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETCELLA            (MC_GM_FIRST + 9)

/**
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are set.
 * If @c lParam is @c NULL, the geometry is reset to a default values.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETGEOMETRY         (MC_GM_FIRST + 10)

/**
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are retrieved.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETGEOMETRY         (MC_GM_FIRST + 11)

/**
 * @brief Requests to redraw region of cells.
 *
 * Note the message just invalidates the region of cells, and the control is
 * not actually repainted until it receives a @c WM_PAINT message.
 *
 * @param[in] wParam Specification of top left cell in the region to be
 * (re)painted. Low word specifies its column, high word specifies its row.
 * @param[in] lParam Specification of bottom right cell in the region to
 * be (re)painted. Low word specifies its column, high word specifies its row.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 *
 * For example, to repaint a single cell, use can use code similar to this:
 * @code
 * SendMessage(hwndGrid, MC_GM_REDRAWCELLS, MAKEWPARAM(wCol, wRow), MAKELPARAM(wCol, wRow));
 * @endcode
 */
#define MC_GM_REDRAWCELLS         (MC_GM_FIRST + 12)

/**
 * @brief Set width of specified column.
 *
 * To reset width of the column to the default value (as specified with
 * @ref MC_GGEOMETRY::wDefColumnWidth), set the column width to @c 0xFFFF.
 *
 * Note this message can set only width of an ordinary grid column.
 * To change the width of row headers, use @ref MC_GM_SETGEOMETRY.
 *
 * @param[in] wParam (@c WORD) Index of the column.
 * @param[in] lParam (@c DWORD) Set low word to the desired width in pixels,
 * high word to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCOLUMNWIDTH      (MC_GM_FIRST + 13)

/**
 * @brief Get width of specified column.
 *
 * @param[in] wParam (@c WORD) Index of the column.
 * @param[in] lParam Reserved, set to zero.
 * @return (@c LRESULT) If the message fails, the return value is @c -1.
 * On success, low word is the width in pixels, high word is reserved for
 * future and it is currently always set to zero.
 */
#define MC_GM_GETCOLUMNWIDTH      (MC_GM_FIRST + 14)

/**
 * @brief Set height of specified row.
 *
 * To reset height of the row to the default value (as specified with
 * @ref MC_GGEOMETRY::wDefRowHeight), set the row height to @c 0xFFFF.
 *
 * Note this message can set only height of an ordinary grid row.
 * To change the height of column headers, use @ref MC_GM_SETGEOMETRY.
 *
 * @param[in] wParam (@c WORD) Index of the row.
 * @param[in] lParam (@c DWORD) Set low word to the desired height in pixels,
 * high word to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETROWHEIGHT        (MC_GM_FIRST + 15)

/**
 * @brief Get height of specified row.
 *
 * @param[in] wParam (@c WORD) Index of the row.
 * @param[in] lParam Reserved, set to zero.
 * @return (@c LRESULT) If the message fails, the return value is @c -1.
 * On success, low word is the height in pixels, high word is reserved for
 * future and it is currently always set to zero.
 */
#define MC_GM_GETROWHEIGHT        (MC_GM_FIRST + 16)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Notification providing a hint for data caching strategy for grids
 * with virtual table.
 *
 * The notification is sent only when the control has the style @ref
 * MC_GS_OWNERDATA. The notification informs the application about region of
 * cells it is likely to ask for (via @ref MC_GN_GETDISPINFO).
 *
 * If the retrieval of assorted data by the application is slow (e.g. because
 * it requires remote access to a database system), the application should
 * cache locally the data for cells in the rectangular cell region as specified
 * by the @c MC_NMGCACHEHINT.
 *
 * The structure never specify header cells, however if the control has style
 * @ref MC_GS_COLUMNHEADERNORMAL and/or @ref MC_GS_ROWHEADERNORMAL, it should
 * also cache data needed for column/row headers corresponding for the cells
 * in the region.
 *
 * For example, if the grid uses the styles @ref MC_GS_COLUMNHEADERNORMAL and
 * @ref MC_GS_ROWHEADERNORMAL, the following data are recommended to be cached
 * within the application:
 *
 * - All ordinary cells in the rectangle with left top cell
 *   [@c columnFrom, @c rowFrom] and right bottom cell [@c columnTo, @c rowTo].
 * - All column header cells in the range @c columnFrom ... @c columnFrom.
 * - All row header cells in the range @c rowFrom ... @c rowTo.
 *
 * In general, the specified cell region roughly corresponds to cells currently
 * visible in the control's window client area.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCACHEHINT*) Pointer to @ref MC_NMGCACHEHINT
 * structure.
 * @return None.
 */
#define MC_GN_ODCACHEHINT         (MC_GN_FIRST + 0)

#if 0  /* TODO */
#define MC_GN_SETDISPINFOW        (MC_GN_FIRST + 1)
#define MC_GN_SETDISPINFOA        (MC_GN_FIRST + 2)
#endif

/**
 * @brief Fired when control needs to retrieve some cell data, the parent holds
 * (Unicode variant).
 *
 * This may happen when @c MC_TABLECELL::pszText was set to the magical value
 * @ref MC_LPSTR_TEXTCALLBACK, or when the control has the style @ref
 * MC_GS_OWNERDATA.
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should fill with the  @c MC_NMGDISPINFO::cell::fMask. Note that the mask
 * can ask for both, the cell text as well the value. However application is
 * then expected to provide one or the other (and set the other one to @c NULL).
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_GETDISPINFOW        (MC_GN_FIRST + 3)

/**
 * @brief Fired when control needs to retrieve some cell data, the parent holds
 * (ANSI variant).
 *
 * This may happen when @c MC_TABLECELL::pszText was set to the magical value
 * @ref MC_LPSTR_TEXTCALLBACK, or when the control has the style @ref
 * MC_GS_OWNERDATA.
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should fill with the  @c MC_NMGDISPINFO::cell::fMask. Note that the mask
 * can ask for both, the cell text as well the value. However application is
 * then expected to provide one or the other (and set the other one to @c NULL).
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_GETDISPINFOA        (MC_GN_FIRST + 4)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_GRIDW MC_WC_GRIDA */
#define MC_WC_GRID          MCTRL_NAME_AW(MC_WC_GRID)
/** Unicode-resolution alias. @sa MC_NMGDISPINFOW MC_NMGDISPINFOA */
#define MC_NMGDISPINFO      MCTRL_NAME_AW(MC_NMGDISPINFO)
/** Unicode-resolution alias. @sa MC_GM_SETCELLW MC_GM_SETCELLA */
#define MC_GM_SETCELL       MCTRL_NAME_AW(MC_GM_SETCELL)
/** Unicode-resolution alias. @sa MC_GM_GETCELLW MC_GM_GETCELLA */
#define MC_GM_GETCELL       MCTRL_NAME_AW(MC_GM_GETCELL)
/** Unicode-resolution alias. @sa MC_GN_SETDISPINFOW MC_GN_SETDISPINFOA */
#define MC_GN_SETDISPINFO   MCTRL_NAME_AW(MC_GN_SETDISPINFO)
/** Unicode-resolution alias. @sa MC_GN_GETDISPINFOW MC_GN_GETDISPINFOA */
#define MC_GN_GETDISPINFO   MCTRL_NAME_AW(MC_GN_GETDISPINFO)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_GRID_H */
