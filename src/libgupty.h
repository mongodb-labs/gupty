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

#include <exception>
#include <string>

#include <boost/bimap.hpp>
#include <boost/container/allocator.hpp>

namespace exception {

class normal_exit : public std::exception {};
class early_exit : public std::exception {};

}  // namespace exception

inline void runtime_assert(bool result, const std::string& msg) {
    if ( ! result) {
        throw std::runtime_error(msg);
    }
}

// FIXME: add an "errno_assert" which is like runtime_assert, but uses strerror.
// It will also need to reset errno prior to evaluating the expression, so it
// will need to be a macro.


inline char* raw_c_str(const std::string& s) {
    return const_cast<char*>(s.c_str());
}



template <class T>
class Enum : public boost::bimap<std::string, T, boost::container::allocator<std::string>> {
public:
    using bimap_type = boost::bimap<std::string, T, boost::container::allocator<std::string>>;
    using value_type = T;

    Enum(std::initializer_list<typename bimap_type::value_type> list, std::string defaultName, T defaultValue)
    : bimap_type(list.begin(), list.end()), _defaultName(defaultName), _defaultValue(defaultValue)
    { }

    std::string operator()(const T& t) {
        if (bimap_type::right.count(t) == 0) {
            return _defaultName;
        }
        return bimap_type::right.at(t);
    }

    T operator()(const std::string& name) {
        if (bimap_type::left.count(name) == 0) {
            return _defaultValue;
        }
        return bimap_type::left.at(name);
    }

private:
    std::string _defaultName;
    T _defaultValue;
};

