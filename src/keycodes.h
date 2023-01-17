/*
 * Copyright 2023, MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#pragma once

#include <map>
#include <string>

constexpr auto KEY_Enter = "Enter";
constexpr auto KEY_Return = "Return";
constexpr auto KEY_Backspace = "Backspace";
constexpr auto KEY_Up = "Up";
constexpr auto KEY_Down = "Down";
constexpr auto KEY_Right = "Right";
constexpr auto KEY_Left = "Left";
constexpr auto KEY_Insert = "Insert";
constexpr auto KEY_Home = "Home";
constexpr auto KEY_PageUp = "PageUp";
constexpr auto KEY_Delete = "Delete";
constexpr auto KEY_End = "End";
constexpr auto KEY_PageDown = "PageDown";

constexpr auto CODE_Enter = "\r";
constexpr auto CODE_Return = "\r";
constexpr auto CODE_Backspace = "\177";
constexpr auto CODE_Up = "\033OA";
constexpr auto CODE_Down = "\033OB";
constexpr auto CODE_Right = "\033OC";
constexpr auto CODE_Left = "\033OD";
constexpr auto CODE_Insert = "\033[2~";
constexpr auto CODE_Home = "\033OH";
constexpr auto CODE_PageUp = "\033[5~";
constexpr auto CODE_Delete = "\033[3~";
constexpr auto CODE_End = "\033OF";
constexpr auto CODE_PageDown = "\033[6~";

constexpr auto CODE_Up_2 = "\033[A";
constexpr auto CODE_Down_2 = "\033[B";
constexpr auto CODE_Right_2 = "\033[C";
constexpr auto CODE_Left_2 = "\033[D";
constexpr auto CODE_Home_2 = "\033[1~";
constexpr auto CODE_End_2 = "\033[4~";

extern const std::map<std::string,std::string> keyCodes;

// Returns the (maximal) number of chars that match one of the multi_char_key sequences.
unsigned int multi_char_keys_match(std::string::const_iterator b, std::string::const_iterator e);

// Returns the (maximal) number of chars that match one of the multi_char_key sequences.
unsigned int multi_char_keys_match(const std::string& s);

