/*
 * Copyright (c) 2010-2013 Martin Mitas
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

#ifndef MCTRL_VALUE_H
#define MCTRL_VALUE_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Polymorphic data (@c MC_HVALUETYPE and @c MC_HVALUE).
 *
 * Some controls are able to cope with data of multiple kinds. For example
 * grid control (@c ref MC_WC_GRID) is able to present a table of cells
 * where each cell contains another kind of data, e.g. string, numbers
 * or some images (either bitmaps or icons) and much more.
 *
 * @c MC_HVALUETYPE and @c MC_HVALUE is exactly the abstraction bringing this
 * power to mCtrl. On one side @c MC_HVALUE can contain various kinds of data
 * (according to some @c MC_HVALUETYPE), on the other side the abstraction
 * allows the grid control to manage the values through a single interface.
 *
 * The type @c MC_HVALUE is just an opaque type. It is in effect a handle of
 * the particular piece of data. Therefore the application code cannot
 * manipulate with the data directly but only through some functions.
 *
 * @c MC_HVALUETYPE is also an opaque type which determines how the values of
 * that type behave. If you are familiar with object oriented programming you
 * can imagine the type is a virtual method table (or vtable in short).
 *
 * After the value is no longer needed, @ref mcValue_Destroy() can be used to
 * release any resources taken by the value.
 *
 *
 * @section sec_value_lifetime Value Life Time
 *
 * As said above, from applications point of view the values are represented
 * by an opaque type @c MC_HVALUE. It is just an handle to the value.
 *
 * It is usually useful to pass the handle here or there in application but
 * the application has to be designed in a way that it is clear who owns the
 * handle, i.e. when it can be safely destroyed.
 *
 * Also note the when an application asks for a value from some control
 * (e.g. grid, @c ref MC_WC_GRID) or a data model (e.g. table, @ref MC_HTABLE),
 * it usually gets a <tt>const MC_HVALUE</tt>. It is directly the handle managed
 * by the control, not its copy. The application should not store the value
 * handle as there is no guaranty when the control or data model respectively
 * will reset the value, or even destroy it.
 *
 * If the application needs to store the information represented by the value,
 * it is supposed to make a duplicate of the value with @ref mcValue_Duplicate(),
 * or call (appropriate) getter value and store the information itself.
 *
 *
 * @section sec_value_builtin Built-in Value Types
 *
 * mCtrl provides value types for some very common kinds of data like integer
 * numbers, strings etc.
 *
 * Values of each of these built-in value types can be created by its factory
 * function. There is also a corresponding getter function which allows to get
 * the data held by the value.
 *
 *
 * @section sec_value_strings String Values
 *
 * There are four value types designed to hold strings, identified by the
 * constants:
 *  -- "Ordinary strings" @ref MC_VALUETYPEID_STRINGW and @ref MC_VALUETYPEID_STRINGA
 *  -- "Immutable strings" @ref MC_VALUETYPEID_IMMSTRINGW and @ref MC_VALUETYPEID_IMMSTRINGA
 *
 * There are also two Unicode/ANSI resolution macros @ref MC_VALUETYPEID_STRING
 * and @ref MC_VALUETYPEID_IMMSTRING, one for the ordinary strings, the letter
 * for the immutable ones.
 *
 * The ordinary strings keep copies of the string buffers used during value
 * creation, while the immutable strings only store pointers to original string
 * buffers and they expect the buffer does not change during lifetime of the
 * value.
 *
 * As it is obvious from the description, the ordinary strings can be used
 * for any strings but can bring a penalty of higher memory usage, while work
 * with the immutable strings make the application responsible to guarantee
 * immutability of the underlying string buffers pointed by them.
 */


/**
 * An opaque type representing the value type.
 */
typedef const void* MC_HVALUETYPE;

/**
 * An opaque type representing the value itself.
 * The application is responsible to remember what's the type of the value.
 */
typedef void* MC_HVALUE;


/**
 * @name ID of Built-in Value Types
 * Use these constants to get corresponding @ref MC_HVALUETYPE with function
 * @ref mcValueType_GetBuiltin().
 */
/*@{*/

#define MC_VALUETYPEID_UNDEFINED        0
/** @brief ID for 32-bit signed integer value type. */
#define MC_VALUETYPEID_INT32            1
/** @brief ID for 32-bit unsigned integer value type. */
#define MC_VALUETYPEID_UINT32           2
/** @brief ID for 64-bit signed integer value type. */
#define MC_VALUETYPEID_INT64            3
/** @brief ID for 64-bit unsigned integer value type. */
#define MC_VALUETYPEID_UINT64           4
/** @brief ID for Unicode string value type. */
#define MC_VALUETYPEID_STRINGW          5
/** @brief ID for ANSI string value type. */
#define MC_VALUETYPEID_STRINGA          6
/** @brief ID for immutable Unicode string value type. */
#define MC_VALUETYPEID_IMMSTRINGW       7
/** @brief ID for immutable ANSI string value type. */
#define MC_VALUETYPEID_IMMSTRINGA       8
/** @brief ID for color RGB triplet. */
#define MC_VALUETYPEID_COLOR            9
/** @brief ID for icon (@c HICON). */
#define MC_VALUETYPEID_ICON            10

/*@}*/


/**
 * @name Value Type Functions
 */
/*@{*/

/**
 * @brief Retrieve Handle of a value type implemented in mCtrl.
 * @param[in] id The identifier of the requested value type.
 * @return The type handle corresponding to the @c id, or @c NULL.
 * if the @c id is not supported.
 */
MC_HVALUETYPE MCTRL_API mcValueType_GetBuiltin(int id);

/*@}*/


/**
 * @name Value Functions
 */
/*@{*/

/**
 * @brief Gets a type of the value.
 * @param[in] hValue The value.
 * @return The value type handle, or @c NULL on failure.
 */
MC_HVALUETYPE MCTRL_API mcValue_Type(const MC_HVALUE hValue);

/**
 * @brief Creates a duplicate of the value.
 * @param[in] hValue The value.
 * @return Duplicate of the value, or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_Duplicate(const MC_HVALUE hValue);

/**
 * @brief Destroys a value.
 * @param[in] hValue The value.
 */
void MCTRL_API mcValue_Destroy(MC_HVALUE hValue);

/*@}*/


/**
 * @name Int32 Value Functions
 */
/*@{*/

/**
 * @brief Int32 factory function.
 * @param[in] iValue The integer.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateInt32(INT iValue);

/**
 * @brief Int32 getter function.
 * @param[in] hValue The value.
 * @return The integer, or @c -1 on failure.
 *
 * @note You may need to call @c GetLastError() to distinguish whether @c -1
 * is the value or an error indication.
 */
INT MCTRL_API mcValue_GetInt32(const MC_HVALUE hValue);

/*@}*/


/**
 * @name UInt32 Value Functions
 */
/*@{*/

/**
 * @brief UInt32 factory function.
 * @param[in] uValue The integer.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateUInt32(UINT uValue);

/**
 * @brief UInt32 getter function.
 * @param[in] hValue The value.
 * @return The integer, or @c 0xffffffff on failure.
 *
 * @note You may need to call @c GetLastError() to distinguish whether
 * @c 0xffffffff is the value or an error indication.
 */
UINT MCTRL_API mcValue_GetUInt32(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Int64 Value Functions
 */
/*@{*/

/**
 * @brief Int64 factory function.
 * @param[in] iValue The integer.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateInt64(INT iValue);

/**
 * @brief Int64 getter function.
 * @param[in] hValue The value.
 * @return The integer, or @c -1 on failure.
 *
 * @note You may need to call @c GetLastError() to distinguish whether @c -1
 * is the value or an error indication.
 */
INT MCTRL_API mcValue_GetInt64(const MC_HVALUE hValue);

/*@}*/


/**
 * @name UInt64 Value Functions
 */
/*@{*/

/**
 * @brief UInt64 factory function.
 * @param[in] uValue The integer.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateUInt64(UINT uValue);

/**
 * @brief UInt64 getter function.
 * @param[in] hValue The value.
 * @return The integer, or @c 0xffffffff on failure.
 *
 * @note You may need to call @c GetLastError() to distinguish whether
 * @c 0xffffffff is the value or an error indication.
 */
UINT MCTRL_API mcValue_GetUInt64(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Unicode String Value Functions
 */
/*@{*/

/**
 * @brief Unicode string factory function.
 * @param[in] lpszStr The string.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateStringW(const WCHAR* lpszStr);

/**
 * @brief Unicode string getter function.
 * @param[in] hValue The value.
 * @return The string, or @c NULL on failure.
 */
const WCHAR* MCTRL_API mcValue_GetStringW(const MC_HVALUE hValue);

/*@}*/


/**
 * @name @c ANSI String Value Functions
 */
/*@{*/

/**
 * @brief ANSI string factory function.
 * @param[in] lpszStr The string.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateStringA(const char* lpszStr);

/**
 * @brief ANSI string getter function.
 * @param[in] hValue The value.
 * @return The string, or @c NULL on failure.
 */
const char* MCTRL_API mcValue_GetStringA(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Unicode Immutable String Value Functions
 */
/*@{*/

/**
 * @brief Unicode immutable string factory function.
 * @param[in] lpszStr The string.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateImmStringW(const WCHAR* lpszStr);

/**
 * @brief Unicode immutable string getter function.
 * @param[in] hValue The value.
 * @return The string, or @c NULL on failure.
 */
const WCHAR* MCTRL_API mcValue_GetImmStringW(const MC_HVALUE hValue);

/*@}*/


/**
 * @name ANSI Immutable String Value Functions
 */
/*@{*/

/**
 * @brief ANSI immutable string factory function.
 * @param[in] lpszStr The string.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateImmStringA(const char* lpszStr);

/**
 * @brief ANSI immutable string getter function.
 * @param[in] hValue The value.
 * @return The string, or @c NULL on failure.
 */
const char* MCTRL_API mcValue_GetImmStringA(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Color Value Functions
 */
/*@{*/

/**
 * @brief Color factory function.
 * @param[in] color The color.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateColor(COLORREF color);

/**
 * @brief Color getter function.
 * @param[in] hValue The value.
 * @return The color, or @ref MC_CLR_NONE on failure.
 */
COLORREF MCTRL_API mcValue_GetColor(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Icon Value Functions
 */
/*@{*/

/**
 * @brief Icon factory function.
 * @param[in] hIcon The icon.
 * @return The value or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcValue_CreateIcon(HICON hIcon);

/**
 * @brief Icon getter function.
 * @param[in] hValue The value.
 * @return The icon, or @c NULL on failure.
 */
HICON MCTRL_API mcValue_GetIcon(const MC_HVALUE hValue);

/*@}*/



/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_VALUETYPEID_STRINGW MC_VALUETYPEID_STRINGA */
#define MC_VALUETYPEID_STRING        MCTRL_NAME_AW(MC_VALUETYPEID_STRING)
/** Unicode-resolution alias. @sa MC_VALUETYPEID_IMMSTRINGW MC_VALUETYPEID_IMMSTRINGA */
#define MC_VALUETYPEID_IMMSTRING     MCTRL_NAME_AW(MC_VALUETYPEID_IMMSTRING)
/** Unicode-resolution alias. @sa mcValue_CreateStringW mcValue_CreateStringA */
#define mcValue_CreateString         MCTRL_NAME_AW(mcValue_CreateString)
/** Unicode-resolution alias. @sa mcValue_GetStringW mcValue_GetStringA */
#define mcValue_GetString            MCTRL_NAME_AW(mcValue_GetString)
/** Unicode-resolution alias. @sa mcValue_CreateImmStringW mcValue_CreateImmStringA */
#define mcValue_CreateImmString      MCTRL_NAME_AW(mcValue_CreateImmString)
/** Unicode-resolution alias. @sa mcValue_GetImmStringW mcValue_GetImmStringA */
#define mcValue_GetImmString         MCTRL_NAME_AW(mcValue_GetImmString)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_VALUE_H */
