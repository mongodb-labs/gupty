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

template <class Action, Action None>
class Keymap : public std::map<std::string, Action> {
public:
    using std::map<std::string, Action>::map;

    Action get(const std::string& key) const {
        if (auto it = this->find(key); it != this->end()) {
            return it->second;
        } else {
            return None;
        }
    }
};

