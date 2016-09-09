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

#ifndef METHOD_H_

#define METHOD_H_

#include <android-base/macros.h>
#include <string>
#include <utils/KeyedVector.h>
#include <vector>

namespace android {

struct Annotation;
struct Formatter;
struct ScalarType;
struct Type;
struct TypedVar;

using AnnotationVector =
        DefaultKeyedVector<std::string, Annotation *>;

struct Method {
    Method(const char *name,
           std::vector<TypedVar *> *args,
           std::vector<TypedVar *> *results,
           bool oneway,
           AnnotationVector *annotations);

    std::string name() const;
    const std::vector<TypedVar *> &args() const;
    const std::vector<TypedVar *> &results() const;
    bool isOneway() const { return mOneway; }
    const AnnotationVector &annotations() const;

    void generateCppSignature(Formatter &out,
                              const std::string &className,
                              bool specifyNamespaces) const;

    static std::string GetArgSignature(const std::vector<TypedVar *> &args,
                                       bool specifyNamespaces);
    static std::string GetJavaArgSignature(const std::vector<TypedVar *> &args);

    const TypedVar* canElideCallback() const;

    void dumpAnnotations(Formatter &out) const;

    bool isJavaCompatible() const;

private:
    std::string mName;
    std::vector<TypedVar *> *mArgs;
    std::vector<TypedVar *> *mResults;
    bool mOneway;
    AnnotationVector *mAnnotationsByName;

    DISALLOW_COPY_AND_ASSIGN(Method);
};

struct TypedVar {
    TypedVar(const char *name, Type *type);

    std::string name() const;
    const Type &type() const;

    bool isJavaCompatible() const;

private:
    std::string mName;
    Type *mType;

    DISALLOW_COPY_AND_ASSIGN(TypedVar);
};

}  // namespace android

#endif  // METHOD_H_

