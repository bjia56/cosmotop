/* Copyright 2025 Brett Jia (dev.bjia56@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#pragma once

// Helper macros for counting arguments
#define COSMOTOP_EXPAND(x) x
#define COSMOTOP_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, NAME, ...) NAME
#define COSMOTOP_COUNT_ARGS(...) COSMOTOP_EXPAND(COSMOTOP_GET_MACRO(__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

// Individual FOR_EACH macros for different argument counts
#define COSMOTOP_FOR_EACH_1(func, arg) func(arg)
#define COSMOTOP_FOR_EACH_2(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_1(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_3(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_2(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_4(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_3(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_5(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_4(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_6(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_5(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_7(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_6(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_8(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_7(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_9(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_8(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_10(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_9(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_11(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_10(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_12(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_11(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_13(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_12(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_14(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_13(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_15(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_14(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_16(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_15(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_17(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_16(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_18(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_17(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_19(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_18(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_20(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_19(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_21(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_20(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_22(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_21(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_23(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_22(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_24(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_23(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_25(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_24(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_26(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_25(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_27(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_26(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_28(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_27(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_29(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_28(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_30(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_29(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_31(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_30(func, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_32(func, arg, ...) func(arg) COSMOTOP_FOR_EACH_31(func, __VA_ARGS__)

// Main FOR_EACH macro that selects the appropriate macro based on argument count
#define COSMOTOP_FOR_EACH(func, ...) \
	COSMOTOP_EXPAND(COSMOTOP_GET_MACRO(__VA_ARGS__, \
		COSMOTOP_FOR_EACH_32, \
		COSMOTOP_FOR_EACH_31, \
		COSMOTOP_FOR_EACH_30, \
		COSMOTOP_FOR_EACH_29, \
		COSMOTOP_FOR_EACH_28, \
		COSMOTOP_FOR_EACH_27, \
		COSMOTOP_FOR_EACH_26, \
		COSMOTOP_FOR_EACH_25, \
		COSMOTOP_FOR_EACH_24, \
		COSMOTOP_FOR_EACH_23, \
		COSMOTOP_FOR_EACH_22, \
		COSMOTOP_FOR_EACH_21, \
		COSMOTOP_FOR_EACH_20, \
		COSMOTOP_FOR_EACH_19, \
		COSMOTOP_FOR_EACH_18, \
		COSMOTOP_FOR_EACH_17, \
		COSMOTOP_FOR_EACH_16, \
		COSMOTOP_FOR_EACH_15, \
		COSMOTOP_FOR_EACH_14, \
		COSMOTOP_FOR_EACH_13, \
		COSMOTOP_FOR_EACH_12, \
		COSMOTOP_FOR_EACH_11, \
		COSMOTOP_FOR_EACH_10, \
		COSMOTOP_FOR_EACH_9, \
		COSMOTOP_FOR_EACH_8, \
		COSMOTOP_FOR_EACH_7, \
		COSMOTOP_FOR_EACH_6, \
		COSMOTOP_FOR_EACH_5, \
		COSMOTOP_FOR_EACH_4, \
		COSMOTOP_FOR_EACH_3, \
		COSMOTOP_FOR_EACH_2, \
		COSMOTOP_FOR_EACH_1)(func, __VA_ARGS__))

// Separator-based variants that insert a separator between function applications
#define COSMOTOP_FOR_EACH_SEP_1(func, sep, arg) func(arg)
#define COSMOTOP_FOR_EACH_SEP_2(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_1(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_3(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_2(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_4(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_3(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_5(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_4(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_6(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_5(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_7(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_6(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_8(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_7(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_9(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_8(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_10(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_9(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_11(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_10(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_12(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_11(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_13(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_12(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_14(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_13(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_15(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_14(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_16(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_15(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_17(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_16(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_18(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_17(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_19(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_18(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_20(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_19(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_21(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_20(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_22(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_21(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_23(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_22(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_24(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_23(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_25(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_24(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_26(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_25(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_27(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_26(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_28(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_27(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_29(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_28(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_30(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_29(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_31(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_30(func, sep, __VA_ARGS__)
#define COSMOTOP_FOR_EACH_SEP_32(func, sep, arg, ...) func(arg) sep COSMOTOP_FOR_EACH_SEP_31(func, sep, __VA_ARGS__)

// Main FOR_EACH_SEP macro that selects the appropriate macro based on argument count
#define COSMOTOP_FOR_EACH_SEP(func, sep, ...) \
	COSMOTOP_EXPAND(COSMOTOP_GET_MACRO(__VA_ARGS__, \
		COSMOTOP_FOR_EACH_SEP_32, \
		COSMOTOP_FOR_EACH_SEP_31, \
		COSMOTOP_FOR_EACH_SEP_30, \
		COSMOTOP_FOR_EACH_SEP_29, \
		COSMOTOP_FOR_EACH_SEP_28, \
		COSMOTOP_FOR_EACH_SEP_27, \
		COSMOTOP_FOR_EACH_SEP_26, \
		COSMOTOP_FOR_EACH_SEP_25, \
		COSMOTOP_FOR_EACH_SEP_24, \
		COSMOTOP_FOR_EACH_SEP_23, \
		COSMOTOP_FOR_EACH_SEP_22, \
		COSMOTOP_FOR_EACH_SEP_21, \
		COSMOTOP_FOR_EACH_SEP_20, \
		COSMOTOP_FOR_EACH_SEP_19, \
		COSMOTOP_FOR_EACH_SEP_18, \
		COSMOTOP_FOR_EACH_SEP_17, \
		COSMOTOP_FOR_EACH_SEP_16, \
		COSMOTOP_FOR_EACH_SEP_15, \
		COSMOTOP_FOR_EACH_SEP_14, \
		COSMOTOP_FOR_EACH_SEP_13, \
		COSMOTOP_FOR_EACH_SEP_12, \
		COSMOTOP_FOR_EACH_SEP_11, \
		COSMOTOP_FOR_EACH_SEP_10, \
		COSMOTOP_FOR_EACH_SEP_9, \
		COSMOTOP_FOR_EACH_SEP_8, \
		COSMOTOP_FOR_EACH_SEP_7, \
		COSMOTOP_FOR_EACH_SEP_6, \
		COSMOTOP_FOR_EACH_SEP_5, \
		COSMOTOP_FOR_EACH_SEP_4, \
		COSMOTOP_FOR_EACH_SEP_3, \
		COSMOTOP_FOR_EACH_SEP_2, \
		COSMOTOP_FOR_EACH_SEP_1)(func, sep, __VA_ARGS__))

// Convenience macros
#define COSMOTOP_APPLY(func, ...) COSMOTOP_FOR_EACH(func, __VA_ARGS__)
#define COSMOTOP_APPLY_COMMA(func, ...) COSMOTOP_FOR_EACH_SEP(func, ,, __VA_ARGS__)
#define COSMOTOP_APPLY_SEMICOLON(func, ...) COSMOTOP_FOR_EACH_SEP(func, ;, __VA_ARGS__)

// Indexed variadic macro applications
#define COSMOTOP_APPLY_WITH_INDEX(macro, ...) \
	COSMOTOP_APPLY_WITH_INDEX_IMPL(macro, 0, __VA_ARGS__)

#define COSMOTOP_APPLY_WITH_INDEX_IMPL(macro, index, arg, ...) \
	macro(arg, index) \
	COSMOTOP_IF_ARGS_APPLY_WITH_INDEX_IMPL(macro, COSMOTOP_INC(index), __VA_ARGS__)

#define COSMOTOP_IF_ARGS_APPLY_WITH_INDEX_IMPL(macro, index, ...) \
	COSMOTOP_IF_ARGS(COSMOTOP_APPLY_WITH_INDEX_IMPL(macro, index, __VA_ARGS__))

#define COSMOTOP_INC(x) COSMOTOP_CONCAT(COSMOTOP_INC_, x)
#define COSMOTOP_INC_0 1
#define COSMOTOP_INC_1 2
#define COSMOTOP_INC_2 3
#define COSMOTOP_INC_3 4
#define COSMOTOP_INC_4 5
#define COSMOTOP_INC_5 6
#define COSMOTOP_INC_6 7
#define COSMOTOP_INC_7 8
#define COSMOTOP_INC_8 9
#define COSMOTOP_INC_9 10
#define COSMOTOP_INC_10 11
#define COSMOTOP_INC_11 12
#define COSMOTOP_INC_12 13
#define COSMOTOP_INC_13 14
#define COSMOTOP_INC_14 15
#define COSMOTOP_INC_15 16
#define COSMOTOP_INC_16 17
#define COSMOTOP_INC_17 18
#define COSMOTOP_INC_18 19
#define COSMOTOP_INC_19 20
#define COSMOTOP_INC_20 21
#define COSMOTOP_INC_21 22
#define COSMOTOP_INC_22 23
#define COSMOTOP_INC_23 24
#define COSMOTOP_INC_24 25
#define COSMOTOP_INC_25 26
#define COSMOTOP_INC_26 27
#define COSMOTOP_INC_27 28
#define COSMOTOP_INC_28 29
#define COSMOTOP_INC_29 30
#define COSMOTOP_INC_30 31
#define COSMOTOP_INC_31 32

#define COSMOTOP_CONCAT(a, b) COSMOTOP_CONCAT_IMPL(a, b)
#define COSMOTOP_CONCAT_IMPL(a, b) a ## b

#define COSMOTOP_IF_ARGS(...) COSMOTOP_IF_ARGS_IMPL(__VA_ARGS__)
#define COSMOTOP_IF_ARGS_IMPL(...) COSMOTOP_IF_ARGS_IMPL_(__VA_ARGS__, ,)
#define COSMOTOP_IF_ARGS_IMPL_(x, ...) COSMOTOP_IF_ARGS_IMPL__(x, __VA_ARGS__)
#define COSMOTOP_IF_ARGS_IMPL__(x, y, ...) y
