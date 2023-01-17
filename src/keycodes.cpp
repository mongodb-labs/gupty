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

#include <set>

#include "keycodes.h"

const std::map<std::string,std::string> keyCodes = {
    { KEY_Enter, CODE_Enter },
    { KEY_Return, CODE_Return },
    { KEY_Backspace, CODE_Backspace },
    { KEY_Up, CODE_Up },
    { KEY_Down, CODE_Down },
    { KEY_Right, CODE_Right },
    { KEY_Left, CODE_Left },
    { KEY_Insert, CODE_Insert },
    { KEY_Home, CODE_Home },
    { KEY_PageUp, CODE_PageUp },
    { KEY_Delete, CODE_Delete },
    { KEY_End, CODE_End },
    { KEY_PageDown, CODE_PageDown },
};


namespace {

// https://stackoverflow.com/questions/3844909/sorting-a-setstring-on-the-basis-of-length/3846589#3846589
struct str_length_decreasing {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        const size_t len_lhs = lhs.length();
        const size_t len_rhs = rhs.length();

        if (len_lhs == len_rhs) {
            return lhs < rhs;
        }
        return len_lhs > len_rhs;
    }
};

std::set<std::string, str_length_decreasing> multi_char_keys = {
    CODE_Backspace,
    CODE_Up,
    CODE_Down,
    CODE_Right,
    CODE_Left,
    CODE_Insert,
    CODE_Home,
    CODE_PageUp,
    CODE_Delete,
    CODE_End,
    CODE_PageDown,

    CODE_Up_2,
    CODE_Down_2,
    CODE_Right_2,
    CODE_Left_2,
    CODE_Home_2,
    CODE_End_2,
};

}  // namespace


// Returns the (maximal) number of chars that match one of the multi_char_key sequences.
unsigned int multi_char_keys_match(std::string::const_iterator b, std::string::const_iterator e) {
    std::string s(b, e);
    for (const auto& multi_char_key : multi_char_keys) {
        if (s.starts_with(multi_char_key)) {
            return multi_char_key.length();
        }
    }
    return 0;
}

// Returns the (maximal) number of chars that match one of the multi_char_key sequences.
unsigned int multi_char_keys_match(const std::string& s) {
    return multi_char_keys_match(s.begin(), s.end());
}

