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

#ifndef MCTRL_CHART_H
#define MCTRL_CHART_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Chart control (@c MC_WC_CHART).
 *
 * The chart control is intended to display large sets of numerical data in a
 * visually illustrative way.
 *
 * @attention The chart control requires @c GDIPLUS.DLL version 1.0 or newer to
 * work correctly. This library was introduced in Windows XP and Windows Server
 * 2003. If your application needs to use this control on Windows 2000, you may
 * need to distribute @c GDIPLUS.DLL along with your application. (Microsoft
 * released @c GDIPLUS.DLL 1.0 as a redistributable for this purpose.)
 *
 * In general, the control is able to hold a multiple series of data (data
 * sets). Each series typically denotes some quantity evolving in time or its
 * dependency on other quantity. In the chart each series is represented with a
 * different color, and accompanied with a legend text.
 *
 * The chart control supports charts of many types, depending on the used
 * control style. The type determines how the control presents the data.
 *
 *
 * @section chart_dataset Data Sets
 *
 * To inset, delete or reset the data set values, use messages
 * @ref MC_CHM_INSERTDATASET, @ref MC_CHM_DELETEDATASET or
 * @ref MC_CHM_DELETEALLDATASETS and @ref MC_CHM_SETDATASET respectively.
 *
 * Data sets can have the data only virtually. That is application can provide
 * to the control only an information the data set exists and how large it is.
 * Whenever the control paints and needs really the data, it asks for them
 * with @ref MC_CHN_GETDISPINFO message. This is useful especially if the data
 * for the chart are really large as it avoids duplication of the data in
 * program memory.
 *
 * To set various other attributes of the data set, you may use
 * @ref MC_CHM_SETDATASETLEGEND to set its legend or @ref MC_CHM_SETDATASETCOLOR
 * to set the color representing the values form the data set.
 *
 *
 * @section chart_axes Axes
 *
 * The control has a concept of two axes, the primary and secondary one.
 * It depends on particular chart type though, whether and how the control uses
 * them. Currently each axis has only a single attribute, a factor exponent,
 * but in future versions more attributes may be added.
 *
 * Usually (for chart types where it gives any sense), the primary axis
 * corresponds to the index of data set and in most cases is displayed as the
 * horizontal (X) axis, and the secondary one corresponds to values in a data
 * set and in most cases is displayed as the vertical (Y) axis. An important
 * exception to this rule of thumb are bar (@ref MC_CHS_BAR) and stacked bar
 * (@ref MC_CHS_STACKEDBAR) charts which are similar to the (stacked) column
 * chart, but with horizontal and vertical axes swapped.
 *
 * The factor exponent is an integer value in the range (@c -9 to @c +9),
 * and it it is used when painting values for the given axis. An integer value
 * is multiplied with <tt>(10 ^ exp)</tt>, where @c ^ marks power and
 * @c exp the exponent.
 *
 * This is especially usefull for charts with non-integer values as negative
 * factor exponent actually turns the data set values into fixed point numbers.
 *
 * For example with factor exponent @c -2, data set with values <tt>{ 5, 100,
 * 101, 102 }</tt> would be displayed as <tt>{ 0.05, 1.00, 1.01, 1.02 }.</tt>
 *
 *
 * @section chart_types Chart Types
 *
 * @attention Different types of chart have different requirements about the
 * data and if the application does not follow these requirements, the chart
 * can just display gibberish, or change the output in future mCtrl versions.
 *
 * The pie chart (@ref MC_CHS_PIE) expects each data set consisting of a single
 * non-negative value. It does not display any axis, but factor exponent of
 * the primary one is used for displaying data set values.
 *
 * @note The pie chart visually differs quite a lot from majority of the other
 * chart types this control provides. Many control attributes settable by
 * control messages actually have no impact to the pie chart.
 *
 * The scatter chart (@ref MC_CHS_SCATTER) expects to have all data set sizes
 * of even count of values. Unlike the other chart types, the sizes of the data
 * sets can differ. The scatter chart interprets each data set as a set of
 * value pairs. Each value with odd index corresponds with the primary (X) axis,
 * and even index with secondary (Y) axis respectively.
 *
 * The line chart (@ref MC_CHS_LINE), the area chart (@ref MC_CHS_AREA), the
 * column chart (@ref MC_CHS_COLUMN) and the bar chart (@ref MC_CHS_BAR) only
 * expect that all the data sets are of the same size.
 *
 * Stacked variants of the charts above (@ref MC_CHS_STACKEDLINE, @ref
 * MC_CHS_STACKEDAREA, @ref MC_CHS_STACKEDCOLUMN and @ref MC_CHS_STACKEDBAR)
 * additionally expect all the data in all data sets are positive. Actually
 * the charts work for negative values, but the result can be confusing for
 * the user.
 *
 *
 * @section std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c WM_GETTEXT
 * - @c WM_SETTEXT
 * - @c CCM_SETNOTIFYWINDOW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 * - @c NM_TOOLTIPSCREATED
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 *
 * Note that the function fails, if @c GDIPLUS.DLL is not available.
 *
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcChart_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcChart_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_CHARTW           L"mCtrl.chart"
/** Window class name (ANSI variant). */
#define MC_WC_CHARTA            "mCtrl.chart"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Pie chart. */
#define MC_CHS_PIE             0x0000
/** @brief Scatter chart. */
#define MC_CHS_SCATTER         0x0001
/** @brief Line chart. */
#define MC_CHS_LINE            0x0002
/** @brief Stacked line chart. */
#define MC_CHS_STACKEDLINE     0x0003
/** @brief Area chart. */
#define MC_CHS_AREA            0x0004
/** @brief Stacked area chart. */
#define MC_CHS_STACKEDAREA     0x0005
/** @brief Column chart. */
#define MC_CHS_COLUMN          0x0006
/** @brief Stacked column chart. */
#define MC_CHS_STACKEDCOLUMN   0x0007
/** @brief Bar chart. */
#define MC_CHS_BAR             0x0008
/** @brief Stacked bar chart. */
#define MC_CHS_STACKEDBAR      0x0009
/** @brief This is not actually a valid style, it's the bit-mask of chart type. */
#define MC_CHS_TYPEMASK        0x003f

/**
 * @brief Disables a tooltip window.
 *
 * When having a tooltip window associated, the control uses to show additional
 * information about the pointed value in the chart. By default the control
 * creates its own control when this style is not set.
 *
 * @sa MC_CHM_SETTOOLTIPS
 */
#define MC_CHS_NOTOOLTIPS      0x0040

/**
 * @brief Enable painting with double buffering.
 *
 * It prevents flickering when the control is being continuously resized.
 */
#define MC_CHS_DOUBLEBUFFER   0x0080

/*@}*/


/**
 * @anchor MC_CHDIF_xxxx
 * @name MC_NMCHDISPINFO::fMask Bits
 */
/*@{*/

/**
 * @brief The control asks for @ref MC_NMCHDISPINFO::piValues.
 *
 * The application is responsible to fill the buffer @c piValues with values
 * of data set determined by @c iDataSet, corresponding to the value indexes
 * in the interval from @c iValueFirst to @c iValueLast (including).
 *
 * The control guarantees the buffer @c piValues is large enough for
 * <tt>(iValueLast-iValueFirst+ 1)</tt> integers.
 */
#define MC_CHDIM_VALUES               (1 << 0)

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure for manipulating with a data set.
 *
 * On input, set @c dwCount to a count of values in the data set (when inserting
 * or setting a data set), or to a number of values which can be written to the
 * buffer pointed by @c piValues (when getting a data set).
 *
 * If inserting or setting a data set and if @c piValues is set to @c NULL, then
 * the control will retrieve the data dynamically from its parent through
 * a notification @ref MC_CHN_GETDISPINFO.
 *
 * @sa MC_CHM_INSERTDATASET MC_CHM_GETDATASET MC_CHM_SETDATASET
 */
typedef struct MC_CHDATASET_tag {
    /** Count of values in the data set. */
    DWORD dwCount;
    /** Pointer to an array of values in the data set. */
    int* piValues;
} MC_CHDATASET;

/**
 * @brief Structure for notification @ref MC_CHN_GETDISPINFO.
 */
typedef struct MC_NMCHDISPINFO_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** Mask of members the control asks for. See @ref MC_CHDIF_xxxx. */
    DWORD fMask;
    /** Data set index. */
    int iDataSet;
    /** Index of a first value the controls asks for. */
    int iValueFirst;
    /** Index of last value the controls asks for. */
    int iValueLast;
    /** Pointer to a buffer where application fills the value. */
    int* piValues;
} MC_NMCHDISPINFO;

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Get count of data sets.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) Count of data sets.
 */
#define MC_CHM_GETDATASETCOUNT        (MC_CHM_FIRST + 0)

/**
 * @brief Delete all data sets.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @sa MC_CHM_DELETEDATASET
 */
#define MC_CHM_DELETEALLDATASETS      (MC_CHM_FIRST + 1)

/**
 * @brief Insert a data set.
 *
 * If the @ref MC_CHDATASET::piValues is set to @c NULL, the control will
 * retrieve the data dynamically from its parent through a notification
 * @ref MC_CHN_GETDISPINFO.
 *
 * @param[in] wParam (@c int) Index of the new data set.
 * @param[in] lParam (@ref MC_CHDATASET*) Pointer to a data of the new data set.
 * @return (@c int) Index of the new data set, or @c -1 on failure.
 */
#define MC_CHM_INSERTDATASET          (MC_CHM_FIRST + 2)

/**
 * @brief Delete a data set.
 * @param[in] wParam (@c int) Index of the data set.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_DELETEDATASET          (MC_CHM_FIRST + 3)

/**
 * @brief Get a data set.
 *
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in,out] lParam (@ref MC_CHDATASET*) Pointer to a data of the new data
 * set. On input, its member @c dwCount must be set to indicate how many values
 * can be written to the buffer pointed by @c piValues. In output, the @c dwCount
 * is updated to a count of values written into the @c piValues. You may also set
 * @c lParam to @c NULL to just get number of values in the data set.
 * @return (@c int) Number of values in the data set, or @c -1 on failure.
 *
 * @note If the data set holds its value only virtually (i.e. if the
 * @c MC_CHDATASET::piValues was @c NULL when the dataset was set), then this
 * message just sets @c MC_CHDATASET::dwCount to zero.
 */
#define MC_CHM_GETDATASET             (MC_CHM_FIRST + 4)

/**
 * @brief Set a data set.
 *
 * If the @c MC_CHDATASET::piValues is set to @c NULL, the control will
 * retrieve the data dynamically from its parent through a notification
 * @ref MC_CHN_GETDISPINFO.
 *
 * @param[in] wParam (@c int) Index of the data set to change.
 * @param[in] lParam (@ref MC_CHDATASET*) Pointer to a data of the new data set.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASET             (MC_CHM_FIRST + 5)

/**
 * @brief Get a color representing the data set in the chart.
 * @param[in] wParam (@c int) Index of the data set.
 * @param lParam Reserved, set to zero.
 * @return (@c COLORREF) @c The color, or @c -1 on failure.
 */
#define MC_CHM_GETDATASETCOLOR        (MC_CHM_FIRST + 6)

/**
 * @brief Set a color representing the data set in the chart.
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c COLORREF) The color. It may be @ref MC_CLR_DEFAULT.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETCOLOR        (MC_CHM_FIRST + 7)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETDATASETLEGENDW      (MC_CHM_FIRST + 8)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETDATASETLEGENDA      (MC_CHM_FIRST + 9)

/**
 * @brief Set legend text of the data set (Unicode variant).
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c WCHAR*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETLEGENDW      (MC_CHM_FIRST + 10)

/**
 * @brief Set legend text of the data set (ANSI variant).
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c char*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETLEGENDA      (MC_CHM_FIRST + 11)

/**
 * @brief Gets factor exponent of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to retrieve the exponent of primary
 * axis, or @c 2 to set secondary axis.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The value of the exponent, or @c -666 on failure.
 */
#define MC_CHM_GETFACTOREXPONENT      (MC_CHM_FIRST + 12)

/**
 * @brief Sets factor exponent of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 0 to set the exponent for both axes,
 * @c 1 to set primary axis, or @c 2 to set secondary axis.
 * @param[in] lParam (@c int) The exponent. Only values from @c -9 to @c +9
 * (inclusive) are allowed.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETFACTOREXPONENT      (MC_CHM_FIRST + 13)

/**
 * @brief Gets offset of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to retrieve the exponent of primary
 * axis, or @c 2 to set secondary axis.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The offset, or @c -666 on failure.
 */
#define MC_CHM_GETAXISOFFSET          (MC_CHM_FIRST + 14)

/**
 * @brief Sets offset of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to set primary axis, or @c 2 to set
 * secondary axis.
 * @param[in] lParam (@c int) The offset.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETAXISOFFSET          (MC_CHM_FIRST + 15)

/**
 * @brief Associate a tooltip window with the chart control.
 * @param[in] wParam (@c HWND) Handle of the tooltip window.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of previous tooltip window or @c NULL if no tooltip
 * was associated with the control.
 * @sa MC_CHS_NOTOOLTIPS
 */
#define MC_CHM_SETTOOLTIPS            (MC_CHM_FIRST + 16)

/**
 * @brief Get tooltip associated with the control
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of the tooltip window or @c NULL if no tooltip
 * is associated with the control.
 */
#define MC_CHM_GETTOOLTIPS            (MC_CHM_FIRST + 17)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETAXISLEGENDW         (MC_CHM_FIRST + 18)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETAXISLEGENDA         (MC_CHM_FIRST + 19)

/**
 * @brief Set legend text of axis (Unicode variant).
 *
 * Note that pie chart does not paint the axis legend.
 *
 * @param[in] wParam (@c int) Set to @c 1 to set primary axis, or @c 2 to set
 * secondary axis.
 * @param[in] lParam (@c WCHAR*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETAXISLEGENDW         (MC_CHM_FIRST + 20)

/**
 * @brief Set legend text of axis (ANSI variant).
 *
 * Note that pie chart does not paint the axis legend.
 *
 * @param[in] wParam (@c int) Set to @c 1 to set primary axis, or @c 2 to set
 * secondary axis.
 * @param[in] lParam (@c char*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETAXISLEGENDA         (MC_CHM_FIRST + 21)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when the control needs some data from its parent.
 *
 * When the control itself holds any data only virtually and it actually needs
 * them, it asks the application to provide them with this message. Application
 * is expected to inspect @c MC_NMCHDISPINFO::fMask and fill the structure
 * with corresponding data.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMCHDISPINFO*) Pointer to structure for
 * exchanging the data.
 * @return Ignored.
 */
#define MC_CHN_GETDISPINFO            (MC_CHN_FIRST + 0)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_CHARTW MC_WC_CHARTA */
#define MC_WC_CHART              MCTRL_NAME_AW(MC_WC_CHART)
/** Unicode-resolution alias. @sa MC_CHM_GETDATASETLEGENDW MC_CHM_GETDATASETLEGENDA */
#define MC_CHM_GETDATASETLEGEND  MCTRL_NAME_AW(MC_CHM_GETDATASETLEGEND)
/** Unicode-resolution alias. @sa MC_CHM_SETDATASETLEGENDW MC_CHM_SETDATASETLEGENDA */
#define MC_CHM_SETDATASETLEGEND  MCTRL_NAME_AW(MC_CHM_SETDATASETLEGEND)
/** Unicode-resolution alias. @sa MC_CHM_GETAXISLEGENDW MC_CHM_GETAXISLEGENDA */
#define MC_CHM_GETAXISLEGEND  MCTRL_NAME_AW(MC_CHM_GETAXISLEGEND)
/** Unicode-resolution alias. @sa MC_CHM_SETAXISLEGENDW MC_CHM_SETAXISLEGENDA */
#define MC_CHM_SETAXISLEGEND  MCTRL_NAME_AW(MC_CHM_SETAXISLEGEND)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_CHART_H */
