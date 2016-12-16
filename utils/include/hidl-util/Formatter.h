/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FORMATTER_H_

#define FORMATTER_H_

#include <android-base/macros.h>
#include <functional>
#include <string>

namespace android {

struct Formatter {
    // Assumes ownership of file. Directed to stdout if file == NULL.
    Formatter(FILE *file = NULL);
    ~Formatter();

    void indent(size_t level = 1);
    void unindent(size_t level = 1);

    // out.indent(2, [&] {
    //     out << "Meow\n";
    // });
    void indent(size_t level, std::function<void(void)> func);
    // out.indent([&] {
    //     out << "Meow\n";
    // });
    void indent(std::function<void(void)> func);

    Formatter &operator<<(const std::string &out);
    Formatter &operator<<(size_t n);

    // Any substrings matching "space" will be stripped out of the output.
    void setNamespace(const std::string &space);

    // Puts a prefix before each line. This is useful if
    // you want to start a // comment block, for example.
    // The prefix will be put before the indentation.
    // Will be effective the next time cursor is at the start of line.
    void setLinePrefix(const std::string& prefix);
    // Remove the line prefix.
    void unsetLinePrefix();

private:
    FILE *mFile;
    size_t mIndentDepth;
    bool mAtStartOfLine;

    std::string mSpace;
    std::string mLinePrefix;

    void output(const std::string &text) const;

    DISALLOW_COPY_AND_ASSIGN(Formatter);
};

}  // namespace android

#endif  // FORMATTER_H_

