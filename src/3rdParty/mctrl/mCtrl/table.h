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

#ifndef MCTRL_TABLE_H
#define MCTRL_TABLE_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>
#include <mCtrl/value.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Table (data model for grid control)
 *
 * The table is actually a container which manages set of values arranged in
 * a two-dimensional matrix. It serves as a back-end for the grid control
 * (@ref MC_WC_GRID).
 *
 *
 * @section table_cell Cell
 *
 * To set or get an information about a cell, application uses the structure
 * @ref MC_TABLECELL. The main data associated with each cell is a text (string)
 * or value (@ref MC_HVALUE). Note the cell can only hold one or the other, but
 * not both.
 *
 * When the cell is holding a string and an application sets the cell to
 * a value, the string is freed. When the cell is holding a value and app sets
 * the cell to a string, then the value is destroyed. The cell holds whatever
 * is set last to it. Any attempt to set both at the same time (i.e. using
 * mask <tt>MC_TCMF_TEXT | MC_TCMF_VALUE</tt> with a setter function) causes
 * a failure of the setter function.
 *
 * When getting a cell and the mask <tt>MC_TCMF_TEXT | MC_TCMF_VALUE</tt> is
 * used, then on output either @c MC_TABLECELL::pszText or
 * @c MC_TABLECELL::hValue is @c NULL, depending on what the cell holds.
 * (Both can be @c NULL if the cell does not neither string nor value.)
 *
 *
 * @section table_header Column and Row Headers
 *
 * The table also holds a cell for each column and row. The grid uses data
 * of these cells as headers for columns and rows (with its default styles;
 * the control provides some styles changing this behavior).
 *
 * The cells are manipulated the same way as ordinary cells. Just to address
 * the header cells, the macro @ref MC_TABLE_HEADER has to be used instead
 * of column index (for row header) or row index (for column header).
 */


/**
 * @brief Opaque table handle.
 */
typedef void* MC_HTABLE;


/**
 * @brief ID of column/row headers.
 *
 * To set or get a contents of column or row header, specify this constant
 * as row/column index.
 *
 * For example, to set value label of a column identified with @c wCol:
 * @code
 * mcTable_SetCell(hTable, wCol, MC_TABLE_HEADER, pCell);
 * @endcode
 */
#define MC_TABLE_HEADER              0xffff


/**
 * @anchor MC_TCMF_xxxx
 * @name MC_TABLECELL::fMask Bits
 */
/*@{*/

/** @brief Set if @ref MC_TABLECELLW::pszText or @ref MC_TABLECELLA::pszText is valid.
 *  @see @ref table_cell */
#define MC_TCMF_TEXT                 0x00000001
/** @brief Set if @ref MC_TABLECELLW::hValue or @ref MC_TABLECELLA::hValue is valid.
 *  @see @ref table_cell */
#define MC_TCMF_VALUE                0x00000002
/** @brief Set if @ref MC_TABLECELLW::lParam or @ref MC_TABLECELLA::lParam is valid. */
#define MC_TCMF_PARAM                0x00000004
/** @brief Set if @ref MC_TABLECELLW::dwFlags or @ref MC_TABLECELLA::dwFlags is valid. */
#define MC_TCMF_FLAGS                0x00000008

/*@}*/


/**
 * @anchor MC_TCF_xxxx
 * @name MC_TABLECELL::dwFlags Bits
 */
/*@{*/

/** @brief Paint the cell value aligned horizontally as default for the value type. */
#define MC_TCF_ALIGNDEFAULT         0x00000000
/** @brief Paint the cell value aligned horizontally to left. */
#define MC_TCF_ALIGNLEFT            0x00000001
/** @brief Paint the cell value aligned horizontally to center. */
#define MC_TCF_ALIGNCENTER          0x00000003
/** @brief Paint the cell value aligned horizontally to right. */
#define MC_TCF_ALIGNRIGHT           0x00000002
/** @brief Paint the cell value aligned vertically as normal for the value type. */
#define MC_TCF_ALIGNVDEFAULT        0x00000000
/** @brief Paint the cell value aligned vertically to top. */
#define MC_TCF_ALIGNTOP             0x00000004
/** @brief Paint the cell value aligned vertically to center. */
#define MC_TCF_ALIGNVCENTER         0x0000000C
/** @brief Paint the cell value aligned vertically to bottom. */
#define MC_TCF_ALIGNBOTTOM          0x00000008

/*@}*/


/**
 * @brief Structure describing a table cell (Unicode variant).
 *
 * Note that only members corresponding to the set bits of the @c fMask
 * are considered valid. (@c fMask itself is always valid of course.)
 *
 * @sa mcTable_SetCell mcTable_GetCell
 */
typedef struct MC_TABLECELLW_tag {
    /** Bitmask specifying what other members are valid. See @ref MC_TCMF_xxxx. */
    DWORD fMask;
    /** Cell text. @see @ref table_cell */
    WCHAR* pszText;
    /** Number of characters in @c pszText. Used only on output. */
    int cchTextMax;
    /** Cell value. @see @ref table_cell */
    MC_HVALUE hValue;
    /** User data. */
    LPARAM lParam;
    /** Cell flags. See @ref MC_TCF_xxxx. */
    DWORD dwFlags;
} MC_TABLECELLW;


/**
 * @brief Structure describing a table cell (ANSI variant).
 *
 * Note that only members corresponding to the set bits of the @c fMask
 * are considered valid. (@c fMask itself is always valid of course.)
 *
 * @sa mcTable_SetCell mcTable_GetCell
 */
typedef struct MC_TABLECELLA_tag {
    /** Bitmask specifying what other members are valid. See @ref MC_TCMF_xxxx. */
    DWORD fMask;
    /** Cell text. @see @ref table_cell */
    char* pszText;
    /** Number of characters in @c pszText. Used only on output. */
    int cchTextMax;
    /** Cell value. @see @ref table_cell */
    MC_HVALUE hValue;
    /** User data. */
    LPARAM lParam;
    /** Cell flags. See @ref MC_TCF_xxxx. */
    DWORD dwFlags;
} MC_TABLECELLA;


/**
 * @name Functions
 */
/*@{*/

/**
 * @brief Create new table.
 *
 * The table is initially empty and has its reference counter set to 1.
 *
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @param dwReserved Reserved. Set to zero.
 * @return Handle of the new table or @c NULL on failure.
 */
MC_HTABLE MCTRL_API mcTable_Create(WORD wColumnCount, WORD wRowCount,
                                   DWORD dwReserved);

/**
 * @brief Increment reference counter of the table.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_AddRef(MC_HTABLE hTable);

/**
 * @brief Decrement reference counter of the table.
 *
 * If the reference counter drops to zero, all resources allocated for
 * the table are released.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Release(MC_HTABLE hTable);

/**
 * @brief Retrieve count of table columns.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_ColumnCount(MC_HTABLE hTable);

/**
 * @brief Retrieve count of table rows.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_RowCount(MC_HTABLE hTable);

/**
 * @brief Resize the table.
 *
 * If a table dimension decreases, the values from excessive cells are destroyed.
 * If a table dimension increases, the new cells are initialized to empty
 * values (in case of heterogeneous table) or to @c NULL values (for homogeneous
 * table).
 *
 * @param[in] hTable The table.
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_Resize(MC_HTABLE hTable, WORD wColumnCount, WORD wRowCount);

/**
 * @brief Clear the table.
 *
 * Clears all cells of the table satisfying the condition as specified by
 * @c dwWhat.
 *
 * @param[in] hTable The table.
 * @param[in] dwWhat Specification of the cells to be cleared. When set to zero,
 * all table contents (including header cells) is cleared. When non-zero, the
 * value is interpreted as a bit-mask of cells to clear:
 * Set bit @c 0x1 to clear all ordinary cells, @c 0x2 to clear column
 * headers and bit @c 0x4 to clear row cells.
 */
void MCTRL_API mcTable_Clear(MC_HTABLE hTable, DWORD dwWhat);

/**
 * @brief Set contents of a cell (Unicode variant).
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] pCell Specifies attributes of the cell to set.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_SetCellW(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                MC_TABLECELLW* pCell);

/**
 * @brief Set contents of a cell (ANSI variant).
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] pCell Specifies attributes of the cell to set.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_SetCellA(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                MC_TABLECELLA* pCell);

/**
 * @brief Get contents of a cell (Unicode variant).
 *
 * Before calling this function, the member @c pCell->fMask must specify what
 * attributes of the cell to retrieve.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[out] pCell Specifies retrieved attributes of the cell.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_GetCellW(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                MC_TABLECELLW* pCell);

/**
 * @brief Get contents of a cell (ANSI variant).
 *
 * Before calling this function, the member @c pCell->fMask must specify what
 * attributes of the cell to retrieve.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[out] pCell Specifies retrieved attributes of the cell.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_GetCellA(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                MC_TABLECELLA* pCell);

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_TABLECELLW MC_TABLECELLA */
#define MC_TABLECELL             MCTRL_NAME_AW(MC_TABLECELL)
/** Unicode-resolution alias. @sa mcTable_SetCellW mcTable_SetCellA */
#define mcTable_SetCell          MCTRL_NAME_AW(mcTable_SetCell)
/** Unicode-resolution alias. @sa mcTable_GetCellW mcTable_GetCellA */
#define mcTable_GetCell          MCTRL_NAME_AW(mcTable_GetCell)

/*@}*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_TABLE_H */
