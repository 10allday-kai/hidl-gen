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

#ifndef ENUM_TYPE_H_

#define ENUM_TYPE_H_

#include "ConstantExpression.h"
#include "NamedType.h"

#include <vector>

namespace android {

struct EnumValue;

struct EnumType : public NamedType {
    EnumType(const char *localName,
             std::vector<EnumValue *> *values,
             Type *storageType = NULL);

    const Type *storageType() const;
    const std::vector<EnumValue *> &values() const;

    const ScalarType *resolveToScalarType() const override;

    bool isEnum() const override;

    std::string getCppType(StorageMode mode,
                           std::string *extra,
                           bool specifyNamespaces) const override;

    std::string getJavaType(
            std::string *extra, bool forInitializer) const override;

    std::string getJavaSuffix() const override;

    std::string getJavaWrapperType() const override;

    void emitReaderWriter(
            Formatter &out,
            const std::string &name,
            const std::string &parcelObj,
            bool parcelObjIsPointer,
            bool isReader,
            ErrorMode mode) const override;

    void emitJavaFieldReaderWriter(
            Formatter &out,
            size_t depth,
            const std::string &parcelName,
            const std::string &blobName,
            const std::string &fieldName,
            const std::string &offset,
            bool isReader) const override;

    status_t emitTypeDeclarations(Formatter &out) const override;

    status_t emitJavaTypeDeclarations(
            Formatter &out, bool atTopLevel) const override;

    status_t emitVtsTypeDeclarations(Formatter &out) const override;
    status_t emitVtsAttributeType(Formatter &out) const override;

    void getAlignmentAndSize(size_t *align, size_t *size) const override;

private:
    void getTypeChain(std::vector<const EnumType *> *out) const;
    std::vector<EnumValue *> *mValues;
    Type *mStorageType;

    DISALLOW_COPY_AND_ASSIGN(EnumType);
};

struct EnumValue {
    EnumValue(const char *name, const ConstantExpression *value = nullptr);

    std::string name() const;
    std::string value() const;
    std::string cppValue(ScalarType::Kind castKind) const;
    std::string javaValue(ScalarType::Kind castKind) const;
    std::string comment() const;

private:
    std::string mName;
    const ConstantExpression *mValue;

    DISALLOW_COPY_AND_ASSIGN(EnumValue);
};

}  // namespace android

#endif  // ENUM_TYPE_H_

