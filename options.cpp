/*
 * Copyright (C) 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "options.h"

#include <cstring>
#include <iostream>
#include <stdio.h>

#include "logging.h"
#include "os.h"

using std::cerr;
using std::endl;
using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace hidl
{
namespace {

unique_ptr<CppOptions> cpp_usage() {
  cerr << "usage: hidl-cpp INPUT_FILE OUTPUT_FILE" << endl
       << endl
       << "OPTIONS:" << endl
      //       << "   -I<DIR>   search path for import statements" << endl
       << "   -d<FILE>  generate dependency file" << endl
       << "   -p        print info from input file (for debugging)" << endl
       << "   -On       generate BnFoo.h" << endl
       << "   -Op       generate BpFoo.h" << endl
       << "   -Oi       generate IFoo.h" << endl
       << "   -Os       generate FooStubs.cpp" << endl
       << "   -Ox       generate FooProxy.cpp" << endl
       << "       (only the last -O option will be used. Default is -Oi.)" << endl
       << endl
       << "INPUT_FILE:" << endl
       << "   a hidl interface file" << endl
       << "OUTPUT_FILE:" << endl
       << "   path to write generated file" << endl;
  return unique_ptr<CppOptions>(nullptr);
}

}  // namespace

unique_ptr<CppOptions> CppOptions::Parse(int argc, const char* const* argv) {
  unique_ptr<CppOptions> options(new CppOptions());
  int i = 1;

  // Parse flags, all of which start with '-'
  for ( ; i < argc; ++i) {
    const size_t len = strlen(argv[i]);
    const char *s = argv[i];
    if (s[0] != '-') {
      break;  // On to the positional arguments.
    }
    if (len < 2) {
      cerr << "Invalid argument '" << s << "'." << endl;
      return cpp_usage();
    }
    const string the_rest = s + 2;
    /*    if (s[1] == 'I') {
      options->import_paths_.push_back(the_rest);
      } else*/
    if (s[1] == 'd') {
      options->dep_file_name_ = the_rest;
    } else if (s[1] == 'p') {
      options->print_stuff_ = true;
    } else if (s[1] == 'O') {
      if (strlen(s) != 3) {
        cerr << "Invalid argument '" << s << "'." << endl;
        return cpp_usage();
      }
      switch(s[2]) {
        case 'p': options->out_type_ = CppOptions::BPFOO; break;
        case 'n': options->out_type_ = CppOptions::BNFOO; break;
        case 'i': options->out_type_ = CppOptions::IFOO; break;
        case 's': options->out_type_ = CppOptions::FOOSTUBS; break;
        case 'x': options->out_type_ = CppOptions::FOOPROXY; break;
        default:
          cerr << "Invalid argument '" << s << "'." << endl;
          return cpp_usage();
      }
    } else {
      cerr << "Invalid argument '" << s << "'." << endl;
      return cpp_usage();
    }
  }

  // There are exactly three positional arguments.
  const int remaining_args = argc - i;
  if (remaining_args != 2) {
    cerr << "Expected 2 positional arguments but got " << remaining_args << "." << endl;
    return cpp_usage();
  }

  options->input_file_name_ = argv[i];
  options->output_file_name_ = argv[i + 1];

  if (!EndsWith(options->input_file_name_, ".hidl")) {
    cerr << "Expected .hidl file for input but got " << options->input_file_name_ << endl;
    return cpp_usage();
  }

  return options;
}

bool EndsWith(const string& str, const string& suffix) {
  if (str.length() < suffix.length()) {
    return false;
  }
  return std::equal(str.crbegin(), str.crbegin() + suffix.length(),
                    suffix.crbegin());
}

bool ReplaceSuffix(const string& old_suffix,
                   const string& new_suffix,
                   string* str) {
  if (!EndsWith(*str, old_suffix)) return false;
  str->replace(str->length() - old_suffix.length(),
               old_suffix.length(),
               new_suffix);
  return true;
}



}  // namespace android
}  // namespace hidl
