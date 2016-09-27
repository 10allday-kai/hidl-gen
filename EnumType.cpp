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

#include "EnumType.h"

#include "ScalarType.h"

#include <assert.h>
#include <inttypes.h>
#include <hidl-util/Formatter.h>
#include <android-base/logging.h>

namespace android {

EnumType::EnumType(
        const char *localName,
        std::vector<EnumValue *> *values,
        Type *storageType)
    : NamedType(localName),
      mValues(values),
      mStorageType(
              storageType != NULL
                ? storageType
                : new ScalarType(ScalarType::KIND_INT32)) {
}

const Type *EnumType::storageType() const {
    return mStorageType;
}

const std::vector<EnumValue *> &EnumType::values() const {
    return *mValues;
}

const ScalarType *EnumType::resolveToScalarType() const {
    return mStorageType->resolveToScalarType();
}

bool EnumType::isEnum() const {
    return true;
}

std::string EnumType::getCppType(StorageMode,
                                 std::string *extra,
                                 bool specifyNamespaces) const {
    extra->clear();

    return specifyNamespaces ? fullName() : partialCppName();
}

std::string EnumType::getJavaType(
        std::string *extra, bool forInitializer) const {
    return mStorageType->resolveToScalarType()->getJavaType(
            extra, forInitializer);
}

std::string EnumType::getJavaSuffix() const {
    return mStorageType->resolveToScalarType()->getJavaSuffix();
}

std::string EnumType::getJavaWrapperType() const {
    return mStorageType->resolveToScalarType()->getJavaWrapperType();
}

void EnumType::emitReaderWriter(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != NULL);

    scalarType->emitReaderWriterWithCast(
            out,
            name,
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode,
            true /* needsCast */);
}

void EnumType::emitJavaFieldReaderWriter(
        Formatter &out,
        size_t depth,
        const std::string &parcelName,
        const std::string &blobName,
        const std::string &fieldName,
        const std::string &offset,
        bool isReader) const {
    return mStorageType->emitJavaFieldReaderWriter(
            out, depth, parcelName, blobName, fieldName, offset, isReader);
}

status_t EnumType::emitTypeDeclarations(Formatter &out) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != NULL);

    std::string extra;

    out << "enum class "
        << localName()
        << " : "
        << ((Type *)scalarType)->getCppType(&extra)
        << " {\n";

    out.indent();

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << entry->name();

            std::string value = entry->cppValue(scalarType->getKind());
            if (!value.empty()) {
                out << " = " << value;
            }

            out << ",";

            std::string comment = entry->comment();
            if (!comment.empty() && comment != value) {
                out << " // " << comment;
            }

            out << "\n";
        }
    }

    out.unindent();
    out << "};\n\n";

    return OK;
}

// Attempt to convert enum value literals into their signed equivalents,
// i.e. if an enum value is stored in typeName 'byte', the value "192"
// will be converted to the output "-64".
static bool MakeSignedIntegerValue(
        const std::string &typeName, const char *value, std::string *output) {
    output->clear();

    char *end;
    long long x = strtoll(value, &end, 10);

    if (end > value && *end == '\0' && errno != ERANGE) {
        char out[32];
        if (typeName == "byte") {
            sprintf(out, "%d", (int)(int8_t)x);
        } else if (typeName == "short") {
            sprintf(out, "%d", (int)(int16_t)x);
        } else if (typeName == "int") {
            sprintf(out, "%d", (int)(int32_t)x);
        } else {
            assert(typeName == "long");
            sprintf(out, "%" PRId64 "L", (int64_t)x);
        }

        *output = out;
        return true;
    }

    return false;
}

status_t EnumType::emitJavaTypeDeclarations(Formatter &out, bool) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != NULL);

    out << "public final class "
        << localName()
        << " {\n";

    out.indent();

    std::string extra;  // unused, because ScalarType leaves this empty.
    const std::string typeName =
        scalarType->getJavaType(&extra, false /* forInitializer */);

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        std::string prevEntryName;
        for (const auto &entry : type->values()) {
            out << "public static final "
                << typeName
                << " "
                << entry->name()
                << " = ";

            std::string value = entry->javaValue(scalarType->getKind());
            if (!value.empty()) {
                std::string convertedValue;
                if (MakeSignedIntegerValue(typeName, value.c_str(), &convertedValue)) {
                    out << convertedValue;
                } else {
                    // The value is not an integer, but some other string,
                    // hopefully referring to some other enum name.
                    out << value;
                }
            } else if (prevEntryName.empty()) {
                out << "0";
            } else {
                out << prevEntryName << " + 1";
            }

            out << ";";

            std::string comment = entry->comment();
            if (!comment.empty() && comment != value) {
                out << " // " << comment;
            }

            out << "\n";

            prevEntryName = entry->name();
        }
    }

    out.unindent();
    out << "};\n\n";

    return OK;
}

status_t EnumType::emitVtsTypeDeclarations(Formatter &out) const {
    out << "name: \"" << localName() << "\"\n"
        << "type: TYPE_ENUM\n"
        << "enum_value: {\n";
    out.indent();

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << "enumerator: \"" << entry->name() << "\"\n";

            std::string value = entry->value();
            if (!value.empty()) {
                out << "value: " << value << "\n";
            }
        }
    }

    out.unindent();
    out << "}\n";
    return OK;
}

status_t EnumType::emitVtsAttributeType(Formatter &out) const {
    out << "type: TYPE_ENUM\n"
        << "predefined_type: \""
        << localName()
        << "\"\n";
    return OK;
}

void EnumType::getTypeChain(std::vector<const EnumType *> *out) const {
    out->clear();
    const EnumType *type = this;
    for (;;) {
        out->push_back(type);

        const Type *superType = type->storageType();
        if (superType == NULL || !superType->isEnum()) {
            break;
        }

        type = static_cast<const EnumType *>(superType);
    }
}

void EnumType::getAlignmentAndSize(size_t *align, size_t *size) const {
    mStorageType->getAlignmentAndSize(align, size);
}

////////////////////////////////////////////////////////////////////////////////

EnumValue::EnumValue(const char *name, const ConstantExpression *value)
    : mName(name),
      mValue(value) {
}

std::string EnumValue::name() const {
    return mName;
}

std::string EnumValue::value() const {
    return mValue ? mValue->value() : "";
}

std::string EnumValue::cppValue(ScalarType::Kind castKind) const {
    return mValue ? mValue->cppValue(castKind) : "";
}
std::string EnumValue::javaValue(ScalarType::Kind castKind) const {
    return mValue ? mValue->javaValue(castKind) : "";
}

std::string EnumValue::comment() const {
    return mValue ? mValue->description() : "";
}

}  // namespace android

