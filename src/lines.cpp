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

#include <algorithm>
#include <cctype>
#include <fstream>

#include "lines.h"

// trim from start (in place)
// https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring/217605#217605
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

Lines readLines(const std::string& filename) {
    Lines lines;
    std::ifstream in(filename);
    std::string line;
    while (std::getline(in, line)) {
        ltrim(line);
        lines.push_back(line);
    }
    return lines;
}

