/*
 * Copyright (c) 2013 Martin Mitas
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

#ifndef MCTRL_TREELIST_H
#define MCTRL_TREELIST_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Tree-list view control (@c MC_WC_TREELIST).
 *
 * Tree-list view control mixes concepts of standard list view control in the
 * report mode (i.e. with the style @c LVS_REPORT) and tree view. As the list
 * view control with the @c LVS_REPORT style, the tree-list is usually divided
 * into multiple columns, which can be manipulated with standard header control
 * (child window of the tree-list control).
 *
 * Unlike the standard list view control, the left-most column resembles the
 * standard tree view control, both in its user experience as well as with its
 * programming interface.
 *
 *
 * @section treelist_columns Columns
 *
 * Usually the very first step after creation of the control, application sets
 * its columns. The messages (@ref MC_TLM_INSERTCOLUMN, @ref MC_TLM_SETCOLUMN
 * etc.) and structure (@ref MC_TLCOLUMN) for this task are very similar to the
 * corresponding messages of the standard list view control.
 *
 * Note however that the tree-list control manages the left-most column
 * (i.e. the column with index 0) a bit specially. This column is always used
 * for displaying the tree-like hierarchy of all items and the control prevents
 * such changes, which would make the column with index 0 on other position
 * then the left-most one (i.e. for column 0, its @c MC_TLCOLUMN::iOrder is
 * always 0 too).
 *
 * Applications attempts to break this rule (e.g. with resetting the order
 * via @ref MC_TLM_SETCOLUMN) will cause the message to fail. The control also
 * supports style @ref MC_TLS_HEADERDRAGDROP which allows user to reorder the
 * columns with mouse, but once again the control prevents change of the order
 * to the left-most column.
 *
 *
 * @section treelist_items_and_subitems Items, Child Items and Subitems
 *
 * Similarly as the standard list view control, the tree-list view control
 * distinguishes between items and subitems. The item (@ref MC_TLITEM) describes
 * state of the item (or row) as a whole, it also determines its position in
 * the tree hierarchy, and finally contains data to be displayed in the
 * left-most column.
 *
 * The subitems (@ref MC_TLSUBITEM) then just hold data for the additional
 * columns of each item (currently only textual label). Note that in this
 * regard the tree-list control differs from list view control where the
 * items as well as the subitems are actually described by single structure
 * @c LVITEM. (The reason behind this is that the item structure is
 * principally different due its tree-like nature.)
 *
 * Like the tree control, each item can have its child items, which are
 * displayed or hidden depending on the expanded state of their parent.
 *
 * Inserting the items into the control is very similar to the standard
 * tree view control. The message @ref MC_TLM_INSERTITEM takes pointer to the
 * structure @ref MC_TLINSERTSTRUCT describing the item as well as its desired
 * position in the tree as its parameters. The message returns an item handle
 * (@ref MC_HTREELISTITEM) representing the new item, and this may be used to
 * set subitems of the item to insert child items (see below) and for other
 * manipulations with the item. Of course there are also special pseudo handles
 * @ref MC_TLI_ROOT, @ref MC_TLI_FIRST and @ref MC_TLI_LAST which fulfill
 * similar role as the tree view counterparts @c TVI_ROOT, @c TVI_FIRST and
 * @c TVI_LAST.
 *
 * Note that when any item is deleted (@ref MC_TLM_DELETEITEM), then the whole
 * subtree of its children is deleted as well.
 *
 *
 * @section treelist_dynamic Dynamically Populated Tree-lists
 *
 * Every single item inserted into the control takes about 40 bytes (32-bit
 * build) or about 80 bytes respectively (64-bit build), excluding label
 * strings and subitems. If you need to create tree hierarchy of huge number of
 * items, it may consume quite large amount of memory.
 *
 * The control addresses this issue by allowing application to populate the
 * control dynamically, ad hoc, when an items are expanded, and optionally
 * to release child items when parent items are collapsed.
 *
 * For huge trees, it is unlikely the user will expand all items, and hence
 * only the expanded items eat the memory.
 *
 * To support this mechanism, the application initially inserts only root
 * items, and sets @c MC_TLITEM::cChildren ti indicate whether the item has
 * children or not.
 *
 * When user attempts to expand the item, the application has to insert the
 * child items dynamically. To achieve this, the application must handle the
 * notification @ref MC_TLN_EXPANDING and check the @ref MC_NMTREELIST::action
 * for @ref MC_TLE_EXPAND.
 *
 * In a similar manner the application may delete the child items if
 * @ref MC_NMTREELIST::action is set to @ref MC_TLE_COLLAPSE. It may do so
 * by sending @ref MC_TLM_EXPAND with @c WPARAM set to
 * <tt>MC_TLE_COLLAPSE | MC_TLE_COLLAPSERESET</tt>.
 *
 *
 * @section treelist_multiselect Multi-selection
 *
 * The control supports selecting multiple items. To enable this feature, use
 * the style @ref MC_TLS_MULTISELECT. However note that only sibling items,
 * i.e. items belonging to the same parent (or root items) can form the
 * multi-selection.
 *
 * Also note that when this style is used, the selection-related notifications
 * @ref MC_TLN_SELCHANGING and @ref MC_TLN_SELCHANGED behave differently. Refer
 * to their respective documentation for more details.
 *
 *
 * @section treelist_callbacks Item and Subitem Callbacks
 *
 * The control can be told to ask parent instead of remembering some attributes
 * of items and subitems. Especially for item and subsystem texts, this technique
 * allows further reduction of memory consumption by the control.
 *
 * To use the callbacks, you can set item or subitem texts to
 * @ref MC_LPSTR_TEXTCALLBACK, any item image to @ref MC_I_IMAGECALLBACK,
 * and/or item @c cChildren member to @ref MC_I_CHILDRENCALLBACK.
 *
 * Whenever the control needs to paint the item/subitem, it then sends a
 * notification @ref MC_TLN_GETDISPINFO (for item) or @ref MC_TLN_GETSUBDISPINFO
 * (for subitem), with @c fMask specifying what members of it the application
 * has to fill.
 *
 *
 * @section treelist_std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_GETUNICODEFORMAT
 * - @c CCM_SETNOTIFYWINDOW
 * - @c CCM_SETUNICODEFORMAT
 * - @c CCM_SETWINDOWTHEME
 *
 * These standard notifications are sent by the control:
 * - @c NM_CLICK
 * - @c NM_CUSTOMDRAW (see @ref MC_NMTLCUSTOMDRAW)
 * - @c NM_DBLCLK
 * - @c NM_KILLFOCUS
 * - @c NM_RCLICK
 * - @c NM_RDBLCLK
 * - @c NM_SETFOCUS
 * - @c NM_OUTOFMEMORY
 * - @c WM_CONTEXTMENU
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 *
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcTreeList_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcTreeList_Terminate(void);

/*@}*/


/**
 * @name Window Class */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_TREELISTW           L"mCtrl.treelist"
/** Window class name (ANSI variant). */
#define MC_WC_TREELISTA            "mCtrl.treelist"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Display expand/collapse buttons next to parent items.
 *  @details To include buttons with root items, application must also use
 *  @ref MC_TLS_LINESATROOT. */
#define MC_TLS_HASBUTTONS           0x0001
/** @brief Use lines to show hierarchy of items. */
#define MC_TLS_HASLINES             0x0002
/** @brief Use lines to link root items.
 *  @details Has no effect if none of @ref MC_TLS_HASBUTTONS and
 *  @ref MC_TLS_HASLINES is set. */
#define MC_TLS_LINESATROOT          0x0004
/** @brief Paint grid lines */
#define MC_TLS_GRIDLINES            0x0008
/** @brief Show selection even when not having a focus. */
#define MC_TLS_SHOWSELALWAYS        0x0010
/** @brief Enable full row select in the control and the entire row of selected
 *  item is highlighted. */
#define MC_TLS_FULLROWSELECT        0x0020
/** @brief Allows item height to be an odd number.
 *  @details If not set, the control rounds odds height to the even value. */
#define MC_TLS_NONEVENHEIGHT        0x0040
/** @brief Use double-buffering when painting. */
#define MC_TLS_DOUBLEBUFFER         0x0080
/** @brief Hide column headers. */
#define MC_TLS_NOCOLUMNHEADER       0x0100
/** @brief Enable column reordering by mouse drag & drop.
 *  @details Note that the left-most column can never be reordered. */
#define MC_TLS_HEADERDRAGDROP       0x0200
/** @brief Selected item is automatically expanded and deselected item is
 *  automatically collapsed.
 *  @details The user can disable this by pressing @<CTRL@> while clicking on
 *  an item. Also note this style is ignored completely if style
 *  @ref MC_TLS_MULTISELECT is set. */
#define MC_TLS_SINGLEEXPAND         0x0400
/** @brief Allow multiple selected items.
 *  @details Note that only sibling items (i.e. items with the same parent item)
 *  can be selected at the same time. */
#define MC_TLS_MULTISELECT          0x0800

#if 0 /* TODO */
#define MC_TLS_NOTOOLTIPS           0x1000
#define MC_TLS_CHECKBOXES           0x2000
#define MC_TLS_EDITLABELS           0x4000
#define MC_TLS_EDITSUBLABELS        0x8000
#endif

/*@}*/


/**
 * @name MC_TLCOLUMN::fMask Bits
 * @anchor MC_TLCF_xxxx
 */
/*@{*/

/** @brief Set if @ref MC_TLCOLUMNW::fmt or @ref MC_TLCOLUMNA::fmt is valid. */
#define MC_TLCF_FORMAT          (1 << 0)
/** @brief Set if @ref MC_TLCOLUMNW::cx or @ref MC_TLCOLUMNA::cx is valid. */
#define MC_TLCF_WIDTH           (1 << 1)
/** @brief Set if @ref MC_TLCOLUMNW::pszText and @c ref MC_TLCOLUMNW::cchTextMax,
 *  or @ref MC_TLCOLUMNA::pszText and @c ref MC_TLCOLUMNA::cchTextMax are valid. */
#define MC_TLCF_TEXT            (1 << 2)
/** @brief Set if @ref MC_TLCOLUMNW::iImage or @ref MC_TLCOLUMNA::iImage is valid. */
#define MC_TLCF_IMAGE           (1 << 3)
/** @brief Set if @ref MC_TLCOLUMNW::iOrder or @ref MC_TLCOLUMNA::iOrder is valid. */
#define MC_TLCF_ORDER           (1 << 4)

/*@}*/


/**
 * @name MC_TLCOLUMN::fmt Bits
 * @anchor MC_TLFMT_xxxx
 */
/*@{*/

/** @brief Text is aligned to left. */
#define MC_TLFMT_LEFT             0x0
/** @brief Text is aligned to right. */
#define MC_TLFMT_RIGHT            0x1
/** @brief Text is centered. */
#define MC_TLFMT_CENTER           0x2
/** @brief A bit-mask of bits for justification. */
#define MC_TLFMT_JUSTIFYMASK      0x3

/*@}*/


/**
 * @name Item Pseudo-handles
 * @anchor MC_TLI_xxxx
 */
/*@{*/

/**
 * @brief Special handle value denoting root item.
 * @details Can be used only where explicitly allowed.
 * @sa MC_TLM_INSERTITEM MC_TLM_DELETEITEM
 */
#define MC_TLI_ROOT  ((MC_HTREELISTITEM)(ULONG_PTR) -0x10000)

/**
 * @brief Special handle first child of a parent item.
 * @details Can be used only where explicitly allowed.
 * @sa MC_TLM_INSERTITEM
 */
#define MC_TLI_FIRST ((MC_HTREELISTITEM)(ULONG_PTR) -0xffff)

/**
 * @brief Special handle last child of a parent item.
 * @details Can be used only where explicitly allowed.
 * @sa MC_TLM_INSERTITEM
 */
#define MC_TLI_LAST  ((MC_HTREELISTITEM)(ULONG_PTR) -0xfffe)

/*@}*/


/**
 * @name MC_TLITEM::fMask Bits
 * @anchor MC_TLIF_xxxx
 */
/*@{*/

/** @brief Set if @c MC_TLITEM::state and @c MC_TLITEM::stateMask is valid. */
#define MC_TLIF_STATE                (1 << 0)
/** @brief Set if @c MC_TLITEM::pszText and @c MC_TLITEM::cchTextMax is valid. */
#define MC_TLIF_TEXT                 (1 << 1)
/** @brief Set if @c MC_TLITEM::lParam is valid. */
#define MC_TLIF_PARAM                (1 << 2)
/** @brief Set if @c MC_TLITEM::lParam is valid.
 *  @note Obsolete. Use @ref MC_TLIF_PARAM. */
#define MC_TLIF_LPARAM               (1 << 2)
/** @brief Set if @c MC_TLITEM::iImage is valid. */
#define MC_TLIF_IMAGE                (1 << 3)
/** @brief Set if @c MC_TLITEM::iSelectedImage is valid. */
#define MC_TLIF_SELECTEDIMAGE        (1 << 4)
/** @brief Set if @c MC_TLITEM::iExpandedImage is valid. */
#define MC_TLIF_EXPANDEDIMAGE        (1 << 5)
/** @brief Set if @c MC_TLITEM::cChildren is valid. */
#define MC_TLIF_CHILDREN             (1 << 6)
/*@}*/


/**
 * @name MC_TLITEM::state Bits
 * @anchor MC_TLIS_xxxx
 */
/*@{*/

/** @brief The item is selected. */
#define MC_TLIS_SELECTED             (1 << 1)
/** @brief The item is expanded. */
#define MC_TLIS_EXPANDED             (1 << 5)
/** @brief The item is highlighted. */


/*@}*/


/**
 * @name MC_TLSUBITEM::fMask Bits
 * @anchor MC_TLSIF_xxxx
 */
/*@{*/

/** @brief Set if @c MC_TLSUBITEM::pszText and @c MC_TLSUBITEM::cchTextMax is
 *  valid. */
#define MC_TLSIF_TEXT                (1 << 1)

/*@}*/


/**
 * @name MC_TLHITTESTINFO::flags Bits
 * @anchor MC_TLHT_xxxx
 */
/*@{*/

/** @brief In the client area, but does not hit any item. */
#define MC_TLHT_NOWHERE              (1 << 0)
/** @brief Never set, reserved for future. */
#define MC_TLHT_ONITEMICON           (1 << 1)
/** @brief Never set, reserved for future. */
#define MC_TLHT_ONITEMSTATEICON      (1 << 2)
/** @brief On (sub)item label. */
#define MC_TLHT_ONITEMLABEL          (1 << 3)
/** @brief On (sub)item */
#define MC_TLHT_ONITEM               (MC_TLHT_ONITEMICON | MC_TLHT_ONITEMSTATEICON | MC_TLHT_ONITEMLABEL)
/** @brief On item indentation. */
#define MC_TLHT_ONITEMINDENT         (1 << 4)
/** @brief On item expand/collapse button. */
#define MC_TLHT_ONITEMBUTTON         (1 << 5)
/** @brief To right of the (sub)item. */
#define MC_TLHT_ONITEMRIGHT          (1 << 6)
/** @brief To left of the (sub)item. This can happen only for subitems in a
 *  column with other then left justification. */
#define MC_TLHT_ONITEMLEFT           (1 << 7)
/** @brief Above the client area. */
#define MC_TLHT_ABOVE                (1 << 8)
/** @brief Below the client area. */
#define MC_TLHT_BELOW                (1 << 9)
/** @brief To right of the client area. */
#define MC_TLHT_TORIGHT              (1 << 10)
/** @brief To left of the client area. */
#define MC_TLHT_TOLEFT               (1 << 11)

/*@}*/


/**
 * @name Action Flags for MC_TLM_EXPAND
 * @anchor MC_TLE_xxxx
 */
/*@{*/

/** @brief Collapse the child items */
#define MC_TLE_COLLAPSE              0x1
/** @brief  Expand the child items */
#define MC_TLE_EXPAND                0x2
/** @brief Collapse the child items if expanded, or expand them if collapsed. */
#define MC_TLE_TOGGLE                0x3
/** @brief Delete all children of the collapsed item.
 *  @details Can be only used together with @ref MC_TLE_EXPAND. */
#define MC_TLE_COLLAPSERESET         0x8000

/*@}*/


/**
 * @name Item Relationship Flags for MC_TLM_GETNEXTITEM
 * @anchor MC_TLGN_xxxx
 */
/*@{*/

/** @brief Get first root item, or @c NULL if there are no items. */
#define MC_TLGN_ROOT                 0x0
/** @brief Get next sibling item of the specified item, or @c NULL if no such
 *  sibling exists. */
#define MC_TLGN_NEXT                 0x1
/** @brief Get previous sibling item of the specified item, or @c NULL if no
 *  such sibling exists. */
#define MC_TLGN_PREVIOUS             0x2
/** @brief Get parent item of of the specified item, or @c NULL if the item is
 *  root. */
#define MC_TLGN_PARENT               0x3
/** @brief Get first child item of the specified item, or @c NULL it the item
 *  has no children. */
#define MC_TLGN_CHILD                0x4
/** @brief Get first visible item, or @c NULL if there are no items.
 *  @details It is the item painted on top of the control, i.e. it depends on
 *  position of vertical scrollbar. */
#define MC_TLGN_FIRSTVISIBLE         0x5
/** @brief Get next visible item, or @c NULL if no visible item follows.
 *  @details The next visible item is the item painted just after the specified
 *  item. I.e. it is determined regardless the tree hierarchy. It does not check
 *  if the next item is in the view-port defined by vertical scrollbar. */
#define MC_TLGN_NEXTVISIBLE          0x6
/** @brief Get previous visible item, or @c NULL if no visible item precedes
 *  the specified item.
 *  @details The previous visible item is the item painted just before the
 *  specified item. I.e. it is determined regardless the tree hierarchy. It does
 *  not check if the previous item is in the view-port defined by vertical
 * scrollbar. */
#define MC_TLGN_PREVIOUSVISIBLE      0x7
/** @brief Get (next) selected item, or @c NULL if no (other) item is selected.
 *  @details If the style @ref MC_TLS_MULTISELECT is used, this can also be
 *  used to retrieve all selected items: To get first selected item, use zero
 *  as @c LPARAM of @ref MC_TLM_GETNEXTITEM. To get next selected item, use
 *  @c non-NULL @c LPARAM. After @ref MC_TLM_GETNEXTITEM returns @c NULL, you
 *  have enumerated over all selected items. */
#define MC_TLGN_CARET                0x9
/** @brief Get last visible item, or @c NULL if there are no items.
 *  @note This is not symmetric to @ref MC_TLGN_FIRSTVISIBLE. It gets the last
 *  item which can be displayed by scrolling. It does not check current state
 *  of scrollbars. */
#define MC_TLGN_LASTVISIBLE          0xa

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure describing column of the tree-list view (Unicode variant).
 * @sa MC_TLM_INSERTCOLUMNW MC_TLM_SETCOLUMNW MC_TLM_GETCOLUMNW
 */
typedef struct MC_TLCOLUMNW_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLCF_xxxx. */
    UINT fMask;
    /** Alignment of the column header and the subitem text in the column.
     *  Note the alignment of leftmost column is always @ref MC_TLFMT_LEFT;
     *  it cannot be changed. See @ref MC_TLFMT_xxxx. */
    int fmt;
    /** Width of the column in pixels. */
    int cx;
    /** Pointer to buffer with column text. */
    WCHAR* pszText;
    /** Size of the buffer pointed by @c pszText. */
    int cchTextMax;
    /** Zero-based index of image in the image list. */
    int iImage;
    /** Zero-based offset of the column (zero indicates the left-most column). */
    int iOrder;
} MC_TLCOLUMNW;

/**
 * @brief Structure describing column of the tree-list view (ANSI variant).
 * @sa MC_TLM_INSERTCOLUMNA MC_TLM_SETCOLUMNA MC_TLM_GETCOLUMNA
 */
typedef struct MC_TLCOLUMNA_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLCF_xxxx. */
    UINT fMask;
    /** Alignment of the column header and the subitem text in the column.
     *  Note the alignment of leftmost column is always @ref MC_TLFMT_LEFT;
     *  it cannot be changed. See @ref MC_TLFMT_xxxx. */
    int fmt;
    /** Width of the column in pixels. */
    int cx;
    /** Pointer to buffer with column text. */
    char* pszText;
    /** Size of the buffer pointed by @c pszText. */
    int cchTextMax;
    /** Zero-based index of image in the image list. */
    int iImage;
    /** Zero-based offset of the column (zero indicates the left-most column). */
    int iOrder;
} MC_TLCOLUMNA;

/**
 * @brief Opaque handle type representing item of the control.
 */
typedef void* MC_HTREELISTITEM;


/**
 * @brief Structure describing item of the tree-list view (Unicode variant).
 * @sa MC_TLM_INSERTITEMW MC_TLM_SETITEMW MC_TLM_GETITEMW
 */
typedef struct MC_TLITEMW_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLIF_xxxx. */
    UINT fMask;
    /** State of the item. See @ref MC_TLIS_xxxx. */
    UINT state;
    /** Mask determining what bits of @c state are valid.
     *  Ignored when getting the item data. */
    UINT stateMask;
    /** The item text. Can be @ref MC_LPSTR_TEXTCALLBACK. */
    WCHAR* pszText;
    /** Size of the buffer pointed with @c pszText.
     *  Ignored when setting the item data. */
    int cchTextMax;
    /** User data. */
    LPARAM lParam;
    /** Image. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iImage;
    /** Image when selected. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iSelectedImage;
    /** Image when expanded. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iExpandedImage;
    /** Flag indicating whether the item has children. When set to 1, control
     *  assumes it has children even though application has not inserted them.
     *  Can be @ref MC_I_CHILDRENCALLBACK */
    int cChildren;
} MC_TLITEMW;

/**
 * @brief Structure describing item of the tree-list view (ANSI variant).
 * @sa MC_TLM_INSERTITEMA MC_TLM_SETITEMA MC_TLM_GETITEMA
 */
typedef struct MC_TLITEMA_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLIF_xxxx. */
    UINT fMask;
    /** State of the item. See @ref MC_TLIS_xxxx. */
    UINT state;
    /** Mask determining what bits of @c state are valid.
     *  Ignored when getting the item data. */
    UINT stateMask;
    /** The item text. Can be @ref MC_LPSTR_TEXTCALLBACK. */
    char* pszText;
    /** Size of the buffer pointed with @c pszText.
     *  Ignored when setting the item data. */
    int cchTextMax;
    /** User data. */
    LPARAM lParam;
    /** Image. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iImage;
    /** Image when selected. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iSelectedImage;
    /** Image when expanded. Can be @ref MC_I_IMAGENONE or @ref MC_I_IMAGECALLBACK. */
    int iExpandedImage;
    /** Flag indicating whether the item has children. When set to 1, control
     *  assumes it has children even though application has not inserted them.
     *  Can be @ref MC_I_CHILDRENCALLBACK */
    int cChildren;
} MC_TLITEMA;

/**
 * @brief Structure describing subitem of the tree-list view (Unicode variant).
 * @sa MC_TLM_SETSUBITEMW MC_TLM_GETSUBITEMW
 */
typedef struct MC_TLSUBITEMW_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLSIF_xxxx. */
    UINT fMask;
    /** ID of subitem to set or get. */
    int iSubItem;
    /** Subitem text. Can be @ref MC_LPSTR_TEXTCALLBACK. */
    WCHAR* pszText;
    /** Size of the buffer pointed with @c pszText.
     *  Ignored when setting the item data. */
    int cchTextMax;
} MC_TLSUBITEMW;

/**
 * @brief Structure describing subitem of the tree-list view (ANSI variant).
 * @sa MC_TLM_SETSUBITEMA MC_TLM_GETSUBITEMA
 */
typedef struct MC_TLSUBITEMA_tag {
    /** Bit-mask specifying what other members are valid.
     *  See @ref MC_TLSIF_xxxx. */
    UINT fMask;
    /** ID of subitem to set or get. */
    int iSubItem;
    /** The subitem text. Can be @ref MC_LPSTR_TEXTCALLBACK. */
    char* pszText;
    /** Size of the buffer pointed with @c pszText.
     *  Ignored when setting the item data. */
    int cchTextMax;
} MC_TLSUBITEMA;

/**
 * @brief Structure used for inserting an item (Unicode variant).
 * @sa MC_TLM_INSERTITEMW
 */
typedef struct MC_TLINSERTSTRUCTW_tag {
    /** Handle of parent item where to insert the item or @ref MC_TLI_ROOT. */
    MC_HTREELISTITEM hParent;
    /** Handle of parent item where to insert the item. Can be @ref MC_TLI_FIRST
     *  or @ref MC_TLI_LAST. */
    MC_HTREELISTITEM hInsertAfter;
    /** The new item data. */
    MC_TLITEMW item;
} MC_TLINSERTSTRUCTW;

/**
 * @brief Structure used for inserting an item (ANSI variant).
 * @sa MC_TLM_INSERTITEMA
 */
typedef struct MC_TLINSERTSTRUCTA_tag {
    /** Handle of parent item where to insert the item or @ref MC_TLI_ROOT. */
    MC_HTREELISTITEM hParent;
    /** Handle of parent item where to insert the item. Can be @ref MC_TLI_FIRST
     *  or @ref MC_TLI_LAST. */
    MC_HTREELISTITEM hInsertAfter;
    /** The new item data. */
    MC_TLITEMA item;
} MC_TLINSERTSTRUCTA;

/**
 * @brief Structure for message @ref MC_MTM_HITTEST.
 */
typedef struct MC_TLHITTESTINFO_tag {
    /** Client coordinate of the point to test. */
    POINT pt;
    /** Flag receiving detail about result of the test. See @ref MC_TLHT_xxxx */
    UINT flags;
    /** Handle of the item that occupies the point. */
    MC_HTREELISTITEM hItem;
    /** Index of the subitem that occupies the point (or zero if it is the
     *  item itself). */
    int iSubItem;
} MC_TLHITTESTINFO;

/**
 * @brief Structure used by many control notifications.
 *
 * Many control notifications use this structure to provide the information
 * about what happens. Refer to documentation of particular messages how it
 * sets the members of the structure. Members not actually used by the
 * notification can be used in future versions of <tt>MCTRL.DLL</tt> so do not
 * rely on their value.
 *
 * If the notification specifies old and/or new item, their its handle and
 * @c lParam is stored. If application needs additional information about the
 * item it has to use @ref MC_TLM_GETITEM message to retrieve it.
 */
typedef struct MC_NMTREELIST_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** Notification specific value. */
    UINT action;
    /** Handle of the old item. */
    MC_HTREELISTITEM hItemOld;
    /** @c lParam of the old item. */
    LPARAM lParamOld;
    /** Handle of the new item. */
    MC_HTREELISTITEM hItemNew;
    /** @c lParam of the new item. */
    LPARAM lParamNew;
} MC_NMTREELIST;

/**
 * @brief Structure used by the standard notification @c NM_CUSTOMDRAW.
 */
typedef struct MC_NMTLCUSTOMDRAW_tag {
    /** Standard custom-draw structure. */
    MC_NMCUSTOMDRAW nmcd;
    /** Level of the item to be painted.
     *  Valid only if <tt>nmcd.dwDrawStage</tt> has the flags @c CDDS_ITEM. */
    int iLevel;
    /** Subitem of the item to be painted.
     *  Valid only if <tt>nmcd.dwDrawStage</tt> has the flags @c CDDS_SUBITEM. */
    int iSubItem;
    /** Item/subitem text color. */
    COLORREF clrText;
    /** Item/subitem background color. */
    COLORREF clrTextBk;
} MC_NMTLCUSTOMDRAW;

/**
 * @brief Structure used by notifications @ref MC_TLN_GETDISPINFO and
 * @ref MC_TLN_SETDISPINFO (Unicode variant).
 */
typedef struct MC_NMTLDISPINFOW_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Handle specifying the item. */
    MC_HTREELISTITEM hItem;
    /** Structure providing the data of the item. */
    MC_TLITEMW item;
} MC_NMTLDISPINFOW;

/**
 * @brief Structure used by notifications @ref MC_TLN_GETDISPINFO and
 * @ref MC_TLN_SETDISPINFO (ANSI variant).
 */
typedef struct MC_NMTLDISPINFOA_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Handle specifying the item. */
    MC_HTREELISTITEM hItem;
    /** Structure providing the data of the item. */
    MC_TLITEMA item;
} MC_NMTLDISPINFOA;

/**
 * @brief Structure used by notifications @ref MC_TLN_GETSUBDISPINFO and
 * @ref MC_TLN_SETSUBDISPINFO (Unicode variant).
 */
typedef struct MC_NMTLSUBDISPINFOW_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Handle specifying the item. */
    MC_HTREELISTITEM hItem;
    /** @c LPARAM of the item. */
    LPARAM lItemParam;
    /** Structure providing the data of the subitem. */
    MC_TLSUBITEMW subitem;
} MC_NMTLSUBDISPINFOW;

/**
 * @brief Structure used by notifications @ref MC_TLN_GETSUBDISPINFO and
 * @ref MC_TLN_SETSUBDISPINFO (ANSI variant).
 */
typedef struct MC_NMTLSUBDISPINFOA_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Handle specifying the item. */
    MC_HTREELISTITEM hItem;
    /** @c LPARAM of the item. */
    LPARAM lItemParam;
    /** Structure providing the data of the subitem. */
    MC_TLSUBITEMA subitem;
} MC_NMTLSUBDISPINFOA;

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Insert a new column (Unicode variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[in] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return Return the index of the new column, or @c -1 on failure.
 */
#define MC_TLM_INSERTCOLUMNW         (MC_TLM_FIRST + 0)

/**
 * @brief Insert a new column (ANSI variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[in] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return Return the index of the new column, or @c -1 on failure.
 */
#define MC_TLM_INSERTCOLUMNA         (MC_TLM_FIRST + 1)

/**
 * @brief Sets attributes of a column (Unicode variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[in] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETCOLUMNW            (MC_TLM_FIRST + 2)

/**
 * @brief Sets attributes of a column (ANSI variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[in] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETCOLUMNA            (MC_TLM_FIRST + 3)

/**
 * @brief Gets attributes of a column (Unicode variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[out] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETCOLUMNW            (MC_TLM_FIRST + 4)

/**
 * @brief Gets attributes of a column (ANSI variant).
 * @param[in] wParam (@c int) Index of the new column.
 * @param[out] lParam (@ref MC_TLCOLUMN*)  Pointer to the column structure.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETCOLUMNA            (MC_TLM_FIRST + 5)

/**
 * @brief Delete a column from the control.
 * @param[in] wParam (@c int) Index of the new column.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_DELETECOLUMN          (MC_TLM_FIRST + 6)

/**
 * @brief Sets left-to-right order of columns.
 * @param[in] wParam (@c int) Size of buffer pointed by @c lParam.
 * @param[out] lParam (@c int*) The array which specify the order of columns
 * from left to right.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETCOLUMNORDERARRAY   (MC_TLM_FIRST + 7)

/**
 * @brief Gets left-to-right order of columns.
 * @param[in] wParam (@c int) Size of buffer pointed by @c lParam.
 * @param[out] lParam (@c int*) The array which receives the column index values.
 * The array must be large enough to hold @c wParam elements.
 * @return (@c int) Count of elements written to the @c lParam, or zero
 * on failure.
 */
#define MC_TLM_GETCOLUMNORDERARRAY   (MC_TLM_FIRST + 8)

/**
 * @brief Changes width of a column.
 * @param[in] wParam (@c int) Index of the new column.
 * @param[in] lParam (@c int) New width of the column in pixels.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETCOLUMNWIDTH        (MC_TLM_FIRST + 9)

/**
 * @brief Gets width of a column.
 * @param[in] wParam (@c int) Index of the new column.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The column width, or zero on failure.
 */
#define MC_TLM_GETCOLUMNWIDTH        (MC_TLM_FIRST + 10)

/**
 * @brief Insert new item into the control (Unicode variant).
 *
 * Note application may set @c MC_TLINSERTSTRUCT::hParent to @ref MC_TLI_ROOT
 * to insert the new item as the root item, and similarly the member
 * @c MC_TLINSERTSTRUCT::hInsertAfter may be set to @c MC_TLI_FIRST or
 * @ref MC_TLI_LAST to insert the item as first or last child item of the
 * parent.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_TLINSERTSTRUCTW*) Pointer to the structure
 * specifying new item position in the tree and other attributes of the item.
 * @return (@ref MC_HTREELISTITEM) Handle of the new item, or @c NULL on failure.
 */
#define MC_TLM_INSERTITEMW           (MC_TLM_FIRST + 11)

/**
 * @brief Insert new item into the control (ANSI variant).
 *
 * Note application may set @c MC_TLINSERTSTRUCT::hParent to @ref MC_TLI_ROOT
 * to insert the new item as the root item, and similarly the member
 * @c MC_TLINSERTSTRUCT::hInsertAfter may be set to @c MC_TLI_FIRST or
 * @ref MC_TLI_LAST to insert the item as first or last child item of the
 * parent.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_TLINSERTSTRUCTA*) Pointer to the structure
 * specifying new item position in the tree and other attributes of the item.
 * @return (@ref MC_HTREELISTITEM) Handle of the new item, or @c NULL on failure.
 */
#define MC_TLM_INSERTITEMA           (MC_TLM_FIRST + 12)

/**
 * @brief Set item attributes (Unicode variant).
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLITEMW*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETITEMW              (MC_TLM_FIRST + 13)

/**
 * @brief Set item attributes (ANSI variant).
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLITEMA*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETITEMA              (MC_TLM_FIRST + 14)

/**
 * @brief Get item attributes (Unicode variant).
 *
 * Application has to set @c MC_TLITEM::fMask prior sending the message to
 * indicate what attributes of the item to retrieve. If the application uses
 * @c MC_TLIF_TEXT, then it also has to set @c MC_TLITEM::pszText to point
 * to a buffer where the text will be stored and set @c MC_TLITEM::cchTextMax
 * to specify size of the buffer.
 *
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[out] lParam (@ref MC_TLITEMW*) Pointer to the structure where the
 * attributes will be written.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETITEMW              (MC_TLM_FIRST + 15)

/**
 * @brief Get item attributes (ANSI variant).
 *
 * Application has to set @c MC_TLITEM::fMask prior sending the message to
 * indicate what attributes of the item to retrieve. If the application uses
 * @c MC_TLIF_TEXT, then it also has to set @c MC_TLITEM::pszText to point
 * to a buffer where the text will be stored and set @c MC_TLITEM::cchTextMax
 * to specify size of the buffer.
 *
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[out] lParam (@ref MC_TLITEMA*) Pointer to the structure where the
 * attributes will be written.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETITEMA              (MC_TLM_FIRST + 16)

/**
 * @brief Delete item from the control.
 *
 * Note the message also deletes all child items recursively, i.e. whole subtree
 * is deleted. If you specify @ref MC_TLI_ROOT as the item to delete, then all
 * items of the control are deleted.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_HTREELISTITEM) Handle of the item or @ref MC_TLI_ROOT.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_DELETEITEM            (MC_TLM_FIRST + 17)

/**
 * @brief Set explicitly height of items.
 *
 * @param[in] wParam (@c int) New height of item in the control. Heights less
 * then zero will be set to 1. If this value is not even and the control does
 * not have the style @ref MC_TLS_NONEVENHEIGHT, it shall be rounded down to
 * the nearest even value. If set to @c -1, the control will revert to default
 * height.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The previous height of the items, in pixels.
 */
#define MC_TLM_SETITEMHEIGHT         (MC_TLM_FIRST + 18)

/**
 * @brief Get height of items.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The height of the items, in pixels.
 */
#define MC_TLM_GETITEMHEIGHT         (MC_TLM_FIRST + 19)

/**
 * @brief Set subitem attributes (Unicode variant).
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLSUBITEMW*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETSUBITEMW           (MC_TLM_FIRST + 20)

/**
 * @brief Set subitem attributes (ANSI variant).
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLSUBITEMW*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_SETSUBITEMA           (MC_TLM_FIRST + 21)

/**
 * @brief Get subitem attributes (Unicode variant).
 *
 * Application has to set @c MC_TLSUBITEM::iSubItem to indicate which subitem
 * it is interested in and @c MC_TLSUBITEM::fMask prior sending the message to
 * indicate what attributes of the item to retrieve. If the application uses
 * @c MC_TLSIF_TEXT, then it also has to set @c MC_TLSUBITEM::pszText to point
 * to a buffer where the text will be stored and set @c MC_TLSUBITEM::cchTextMax
 * to specify size of the buffer.
 *
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLSUBITEMW*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETSUBITEMW           (MC_TLM_FIRST + 22)

/**
 * @brief Get subitem attributes (ANSI variant).
 *
 * Application has to set @c MC_TLSUBITEM::iSubItem to indicate which subitem
 * it is interested in and @c MC_TLSUBITEM::fMask prior sending the message to
 * indicate what attributes of the item to retrieve. If the application uses
 * @c MC_TLSIF_TEXT, then it also has to set @c MC_TLSUBITEM::pszText to point
 * to a buffer where the text will be stored and set @c MC_TLSUBITEM::cchTextMax
 * to specify size of the buffer.
 *
 * @param[in] wParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @param[in] lParam (@ref MC_TLSUBITEMA*) Pointer to the structure
 * specifying new attributes of the item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_GETSUBITEMA           (MC_TLM_FIRST + 23)

/**
 * @brief Set item indentation.
 * @param[in] wParam (@c int) The indentation, in pixels.
 * @param lParam Reserved, set to zero.
 * @return None.
 */
#define MC_TLM_SETINDENT             (MC_TLM_FIRST + 24)

/**
 * @brief Get item indentation.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The indentation, in pixels.
 */
#define MC_TLM_GETINDENT             (MC_TLM_FIRST + 25)

/**
 * @brief Tests which item or subitem (and its part) is placed on specified
 * position.
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_TLHITTESTINFO*) Pointer to a hit test
 * structure. Set @ref MC_TLHITTESTINFO::pt on input.
 * @return (@ref MC_HTREELISTITEM) Handle of the hit item, or @c NULL.
 */
#define MC_TLM_HITTEST               (MC_TLM_FIRST + 26)

/**
 * @brief Expand or collapse child items.
 * @param[in] wParam (@c int) Action flag. See @ref MC_TLE_xxxx.
 * @param[in] lParam (@ref MC_HTREELISTITEM) Handle of the parent item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_TLM_EXPAND                (MC_TLM_FIRST + 27)

/**
 * @brief Get item of the specified relationship to a specified item.
 * @param[in] wParam (@c int) Flag determining the item to retrieve.
 * See @ref MC_TLGN_xxxx.
 * @param[in] lParam (@ref MC_HTREELISTITEM) Handle of an item.
 * @return (@ref MC_HTREELISTITEM) Handle of the item in the specified
 * relationship, or @c NULL.
 */
#define MC_TLM_GETNEXTITEM           (MC_TLM_FIRST + 28)

/**
 * @brief Gets count of items which currently fit into the client area.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The count of items.
 */
#define MC_TLM_GETVISIBLECOUNT       (MC_TLM_FIRST + 29)

/**
 * @brief Ensure the item is visible.
 * @details The message can expand parent items or scroll if necessary.
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_HTREELISTITEM) Handle of the item.
 * @return (@c BOOL) @c TRUE if the control scrolled the items  and no items
 * were expanded, @c FALSE otherwise.
 */
#define MC_TLM_ENSUREVISIBLE         (MC_TLM_FIRST + 30)

/**
 * @brief Associates an image list with the control.
 *
 * Note the control does not delete the previously associated image list.
 * It also does not delete the currently associated image list when destroying
 * the control.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c HIMAGELIST) Handle to the image list.
 * @return (@c HIMAGELIST) Image list handle previously associated with
 * the control.
 */
#define MC_TLM_SETIMAGELIST          (MC_TLM_FIRST + 31)

/**
 * @brief Gets an image list associated with the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c HIMAGELIST) Image list handle associated with the control.
 */
#define MC_TLM_GETIMAGELIST          (MC_TLM_FIRST + 32)

/**
 * @brief Returns the number of currently selected items.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The number of items selected.
 */
#define MC_TLM_GETSELECTEDCOUNT     (MC_TLM_FIRST + 33)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when deleting an item.
 *
 * The members @c hItemOld and @c lParamOld of @ref MC_NMTREELIST specify which
 * item is being deleted.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMTREELIST*) Pointer to a @ref MC_NMTREELIST
 * structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_TLN_DELETEITEM            (MC_TLN_FIRST + 0)

/**
 * @brief Fired when a selection item is about to change.
 *
 * When style @ref MC_TLS_MULTISELECT is not used, @ref MC_NMTREELIST describes
 * how the selection changes: The members @c hItemOld and @c lParamOld
 * describe the current selection, and the members @c hItemNew and @c lParamNew
 * specify the to-be-selection.
 *
 * However when style @ref MC_TLS_MULTISELECT is used, the notification behaves
 * differently: The notification is only sent for items which are going to be
 * selected (once per such item), and the members @c hItemOld and @c lParamOld
 * are always set to @c NULL.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMTREELIST*) Pointer to a @ref MC_NMTREELIST
 * structure.
 * @return Application may prevent @c TRUE to prevent the selection change,
 * or @c FALSE otherwise to allow it.
 */
#define MC_TLN_SELCHANGING           (MC_TLN_FIRST + 1)

/**
 * @brief Fired when selection has changed.
 *
 * When style @ref MC_TLS_MULTISELECT is not used, @ref MC_NMTREELIST describes
 * how the selection has changed: The members @c hItemOld and @c lParamOld
 * describe the old selection, and the members @c hItemNew and @c lParamNew
 * specify the new selection.
 *
 * However when style @ref MC_TLS_MULTISELECT is used, the notification behaves
 * differently: It sends notification just once for the control and all @ref
 * MC_NMTREELIST members are set to @c NULL. Application can iterate through
 * currently selected items by using the message @ref MC_TLM_GETNEXTITEM.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMTREELIST*) Pointer to a @ref MC_NMTREELIST
 * structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_TLN_SELCHANGED            (MC_TLN_FIRST + 2)

/**
 * @brief Fired when a parent item is about to expand or collapse.
 *
 * The members @c hItemNew and @c lParamNew of @ref MC_NMTREELIST specify the
 * item which is changing its state. The member @c action is set to @ref
 * MC_TLE_EXPAND or @ref MC_TLE_COLLAPSE to specify that the item is going
 * to expand or collapse respectively.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMTREELIST*) Pointer to a @ref MC_NMTREELIST
 * structure.
 * @return Application may prevent @c TRUE to prevent the item state change,
 * or @c FALSE otherwise to allow it.
 */
#define MC_TLN_EXPANDING             (MC_TLN_FIRST + 3)

/**
 * @brief Fired when a parent item has expanded or collapsed.
 *
 * The members @c hItemNew and @c lParamNew of @ref MC_NMTREELIST specify
 * the item which has changed its state, The member @c action is set to
 * @ref MC_TLE_EXPAND or @ref MC_TLE_COLLAPSE to specify that the item has
 * expanded or collapsed respectively.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMTREELIST*) Pointer to a @ref MC_NMTREELIST
 * structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_TLN_EXPANDED              (MC_TLN_FIRST + 4)

#if 0
#define MC_TLN_SETDISPINFOW          (MC_TLN_FIRST + 5)
#define MC_TLN_SETDISPINFOA          (MC_TLN_FIRST + 6)
#endif

/**
 * @brief Fired when control needs to retrieve some item data, the parent holds
 * (Unicode variant).
 *
 * This may happen when some members of an item were set to a callback magical
 * value such as @ref MC_LPSTR_TEXTCALLBACK, @ref MC_I_IMAGECALLBACK and
 * @ref MC_I_CHILDRENCALLBACK.
 *
 * When sending the notification, the control sets @c MC_NMTLDISPINFO::hItem
 * to the handle of the item it is interested in, @c MC_NMTLDISPINFO::item::fMask
 * to specify what members of @c MC_NMTLDISPINFO::item the application is
 * supposed to fill, and it also sets @c MC_NMTLDISPINFO::item::lParam
 * (this is for convenience of the application, as often the app stores some
 * id needed for retrieving the requested data).
 *
 * The parent must fill the data in @c MC_NMTLDISPINFO::item, as requested
 * by the mask @c MC_NMTLDISPINFO::item::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMTLDISPINFO*) Pointer to a @ref MC_NMTLDISPINFO
 * structure.
 * @return None.
 */
#define MC_TLN_GETDISPINFOW          (MC_TLN_FIRST + 7)

/**
 * @brief Fired when control needs to retrieve some item data, the parent holds
 * (ANSI variant).
 *
 * This may happen when some members of an item were set to a callback magical
 * value such as @ref MC_LPSTR_TEXTCALLBACK, @ref MC_I_IMAGECALLBACK and
 * @ref MC_I_CHILDRENCALLBACK.
 *
 * When sending the notification, the control sets @c MC_NMTLDISPINFO::hItem
 * to the handle of the item it is interested in, @c MC_NMTLDISPINFO::item::fMask
 * to specify what members of @c MC_NMTLDISPINFO::item the application is
 * supposed to fill, and it also sets @c MC_NMTLDISPINFO::item::lParam
 * (this is for convenience of the application, as often the app stores some
 * id needed for retrieving the requested data).
 *
 * The parent must fill the data in @c MC_NMTLDISPINFO::item, as requested
 * by the mask @c MC_NMTLDISPINFO::item::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMTLDISPINFO*) Pointer to a @ref MC_NMTLDISPINFO
 * structure.
 * @return None.
 */
#define MC_TLN_GETDISPINFOA          (MC_TLN_FIRST + 8)

#if 0
#define MC_TLN_SETSUBDISPINFOW       (MC_TLN_FIRST + 9)
#define MC_TLN_SETSUBDISPINFOA       (MC_TLN_FIRST + 10)
#endif

/**
 * @brief Fired when control needs to retrieve some subitem data, the parent
 * holds (Unicode variant).
 *
 * This may happen when the subitem text was set to @ref MC_LPSTR_TEXTCALLBACK.
 *
 * When sending the notification, the control sets @c MC_NMTLDISPINFO::hItem
 * to the handle of the item in whose subitem it is interested in,
 * @c MC_NMTLDISPINFO::subitem.iSubItem to specify what subitem it needs,
 * @c MC_NMTLDISPINFO::subitem::fMask to specify what members of
 * @c MC_NMTLSUBDISPINFO::subitem the application is supposed to fill, and it
 * also sets @c MC_NMTLDISPINFO::lItemParam (this is for convenience of the
 * application, as often the app stores some id needed for retrieving the
 * requested data).
 *
 * The parent must fill the data in @c MC_NMTLSUBDISPINFO::subitem, as requested
 * by the mask @c MC_NMTLSUBDISPINFO::subitem::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMTLSUBDISPINFO*) Pointer to a @ref
 * MC_NMTLSUBDISPINFO structure.
 * @return None.
 */
#define MC_TLN_GETSUBDISPINFOW       (MC_TLN_FIRST + 11)

/**
 * @brief Fired when control needs to retrieve some subitem data, the parent
 * holds (ANSI variant).
 *
 * This may happen when the subitem text was set to @ref MC_LPSTR_TEXTCALLBACK.
 *
 * When sending the notification, the control sets @c MC_NMTLDISPINFO::hItem
 * to the handle of the item in whose subitem it is interested in,
 * @c MC_NMTLDISPINFO::subitem.iSubItem to specify what subitem it needs,
 * @c MC_NMTLDISPINFO::subitem::fMask to specify what members of
 * @c MC_NMTLSUBDISPINFO::subitem the application is supposed to fill, and it
 * also sets @c MC_NMTLDISPINFO::lItemParam (this is for convenience of the
 * application, as often the app stores some id needed for retrieving the
 * requested data).
 *
 * The parent must fill the data in @c MC_NMTLSUBDISPINFO::subitem, as requested
 * by the mask @c MC_NMTLSUBDISPINFO::subitem::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMTLSUBDISPINFO*) Pointer to a @ref
 * MC_NMTLSUBDISPINFO structure.
 * @return None.
 */
#define MC_TLN_GETSUBDISPINFOA       (MC_TLN_FIRST + 12)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_TREELISTW MC_WC_TREELISTA */
#define MC_WC_TREELIST           MCTRL_NAME_AW(MC_WC_TREELIST)
/** Unicode-resolution alias. @sa MC_TLCOLUMNW MC_TLCOLUMNA */
#define MC_TLCOLUMN              MCTRL_NAME_AW(MC_TLCOLUMN)
/** Unicode-resolution alias. @sa MC_TLITEMW MC_TLITEMA */
#define MC_TLITEM                MCTRL_NAME_AW(MC_TLITEM)
/** Unicode-resolution alias. @sa MC_TLSUBITEMW MC_TLSUBITEMA */
#define MC_TLSUBITEM             MCTRL_NAME_AW(MC_TLSUBITEM)
/** Unicode-resolution alias. @sa MC_TLINSERTSTRUCTW MC_TLINSERTSTRUCTA */
#define MC_TLINSERTSTRUCT        MCTRL_NAME_AW(MC_TLINSERTSTRUCT)
/** Unicode-resolution alias. @sa MC_NMTLDISPINFOW MC_NMTLDISPINFOA */
#define MC_NMTLDISPINFO          MCTRL_NAME_AW(MC_NMTLDISPINFO)
/** Unicode-resolution alias. @sa MC_NMTLSUBDISPINFOW MC_NMTLSUBDISPINFOA */
#define MC_NMTLSUBDISPINFO       MCTRL_NAME_AW(MC_NMTLSUBDISPINFO)
/** Unicode-resolution alias. @sa MC_TLM_SETCOLUMNW MC_TLM_SETCOLUMNA */
#define MC_TLM_SETCOLUMN         MCTRL_NAME_AW(MC_TLM_SETCOLUMN)
/** Unicode-resolution alias. @sa MC_TLM_INSERTCOLUMNW MC_TLM_INSERTCOLUMNA */
#define MC_TLM_INSERTCOLUMN      MCTRL_NAME_AW(MC_TLM_INSERTCOLUMN)
/** Unicode-resolution alias. @sa MC_TLM_SETCOLUMNW MC_TLM_SETCOLUMNA */
#define MC_TLM_SETCOLUMN         MCTRL_NAME_AW(MC_TLM_SETCOLUMN)
/** Unicode-resolution alias. @sa MC_TLM_GETCOLUMNW MC_TLM_GETCOLUMNA */
#define MC_TLM_GETCOLUMN         MCTRL_NAME_AW(MC_TLM_GETCOLUMN)
/** Unicode-resolution alias. @sa MC_TLM_INSERTITEMW MC_TLM_INSERTITEMA */
#define MC_TLM_INSERTITEM        MCTRL_NAME_AW(MC_TLM_INSERTITEM)
/** Unicode-resolution alias. @sa MC_TLM_SETITEMW MC_TLM_SETITEMA */
#define MC_TLM_SETITEM           MCTRL_NAME_AW(MC_TLM_SETITEM)
/** Unicode-resolution alias. @sa MC_TLM_GETITEMW MC_TLM_GETITEMA */
#define MC_TLM_GETITEM           MCTRL_NAME_AW(MC_TLM_GETITEM)
/** Unicode-resolution alias. @sa MC_TLM_SETSUBITEMW MC_TLM_SETSUBITEMA */
#define MC_TLM_SETSUBITEM        MCTRL_NAME_AW(MC_TLM_SETSUBITEM)
/** Unicode-resolution alias. @sa MC_TLM_GETSUBITEMW MC_TLM_GETSUBITEMA */
#define MC_TLM_GETSUBITEM        MCTRL_NAME_AW(MC_TLM_GETSUBITEM)
/** Unicode-resolution alias. @sa MC_TLN_SETDISPINFOW MC_TLN_SETDISPINFOA */
#define MC_TLN_SETDISPINFO       MCTRL_NAME_AW(MC_TLN_SETDISPINFO)
/** Unicode-resolution alias. @sa MC_TLN_GETDISPINFOW MC_TLN_GETDISPINFOA */
#define MC_TLN_GETDISPINFO       MCTRL_NAME_AW(MC_TLN_GETDISPINFO)
/** Unicode-resolution alias. @sa MC_TLN_SETSUBDISPINFOW MC_TLN_SETSUBDISPINFOA */
#define MC_TLN_SETSUBDISPINFO    MCTRL_NAME_AW(MC_TLN_SETSUBDISPINFO)
/** Unicode-resolution alias. @sa MC_TLN_GETSUBDISPINFOW MC_TLN_GETSUBDISPINFOA */
#define MC_TLN_GETSUBDISPINFO    MCTRL_NAME_AW(MC_TLN_GETSUBDISPINFO)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_TREELIST_H */
