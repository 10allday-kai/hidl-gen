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

#include "Annotation.h"
#include "ScalarType.h"

#include <inttypes.h>
#include <hidl-util/Formatter.h>
#include <android-base/logging.h>

namespace android {

EnumType::EnumType(
        const char *localName,
        const Location &location,
        Type *storageType)
    : Scope(localName, location),
      mValues(),
      mStorageType(
              storageType != NULL
                ? storageType
                : new ScalarType(ScalarType::KIND_INT32)) {
}

const Type *EnumType::storageType() const {
    return mStorageType;
}

const std::vector<EnumValue *> &EnumType::values() const {
    return mValues;
}

void EnumType::addValue(EnumValue *value) {
    CHECK(value != nullptr);

    EnumValue *prev = nullptr;
    std::vector<const EnumType *> chain;
    getTypeChain(&chain);
    for (auto it = chain.begin(); it != chain.end(); ++it) {
        const auto &type = *it;
        if(!type->values().empty()) {
            prev = type->values().back();
            break;
        }
    }

    value->autofill(prev, resolveToScalarType());
    mValues.push_back(value);
}

const ScalarType *EnumType::resolveToScalarType() const {
    return mStorageType->resolveToScalarType();
}

bool EnumType::isEnum() const {
    return true;
}

std::string EnumType::getCppType(StorageMode,
                                 bool specifyNamespaces) const {
    return specifyNamespaces ? fullName() : partialCppName();
}

std::string EnumType::getJavaType(bool forInitializer) const {
    return mStorageType->resolveToScalarType()->getJavaType(forInitializer);
}

std::string EnumType::getJavaSuffix() const {
    return mStorageType->resolveToScalarType()->getJavaSuffix();
}

std::string EnumType::getJavaWrapperType() const {
    return mStorageType->resolveToScalarType()->getJavaWrapperType();
}

std::string EnumType::getVtsType() const {
    return "TYPE_ENUM";
}

LocalIdentifier *EnumType::lookupIdentifier(const std::string &name) const {
    std::vector<const EnumType *> chain;
    getTypeChain(&chain);
    for (auto it = chain.begin(); it != chain.end(); ++it) {
        const auto &type = *it;
        for(EnumValue *v : type->values()) {
            if(v->name() == name) {
                return v;
            }
        }
    }
    return nullptr;
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
    CHECK(scalarType != nullptr);

    const std::string storageType = scalarType->getCppStackType();

    out << "enum class "
        << localName()
        << " : "
        << storageType
        << " {\n";

    out.indent();

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << entry->name();

            std::string value = entry->cppValue(scalarType->getKind());
            CHECK(!value.empty()); // use autofilled values for c++.
            out << " = " << value;

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

void EnumType::emitEnumBitwiseOrOperator(Formatter &out, bool mutating) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != nullptr);

    const std::string storageType = scalarType->getCppStackType();

    out << "inline "
        << fullName()
        << (mutating ? " &" : "")
        << " operator|"
        << (mutating ? "=" : "")
        << "(\n";

    out.indent();
    out.indent();

    out << fullName()
        << (mutating ? " &" : " ")
        << "lhs, "
        << fullName()
        << " rhs) {\n";
    out.unindent();

    if (mutating) {
        out << "lhs = ";
    } else {
        out << "return ";
    }
    out << "static_cast<"
        << fullName()
        << ">(\n";
    out.indent();
    out.indent();
    out << "static_cast<"
        << storageType
        << ">(lhs) | static_cast<"
        << storageType
        << ">(rhs));\n";
    out.unindent();
    out.unindent();

    if (mutating) {
        out << "return lhs;\n";
    }

    out.unindent();

    out << "}\n\n";
}

status_t EnumType::emitGlobalTypeDeclarations(Formatter &out) const {
    emitEnumBitwiseOrOperator(out, false /* mutating */);
    emitEnumBitwiseOrOperator(out, true /* mutating */);

    return OK;
}

status_t EnumType::emitJavaTypeDeclarations(Formatter &out, bool) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != NULL);

    out << "public final class "
        << localName()
        << " {\n";

    out.indent();

    const std::string typeName =
        scalarType->getJavaType(false /* forInitializer */);

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << "public static final "
                << typeName
                << " "
                << entry->name()
                << " = ";

            // javaValue will make the number signed.
            std::string value = entry->javaValue(scalarType->getKind());
            CHECK(!value.empty()); // use autofilled values for java.
            out << value;

            out << ";";

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

status_t EnumType::emitVtsTypeDeclarations(Formatter &out) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();

    out << "name: \"" << fullName() << "\"\n";
    out << "type: " << getVtsType() << "\n";
    out << "enum_value: {\n";
    out.indent();

    out << "scalar_type: \""
        << scalarType->getVtsScalarType()
        << "\"\n\n";
    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << "enumerator: \"" << entry->name() << "\"\n";
            out << "scalar_value: {\n";
            out.indent();
            // use autofilled values for vts.
            std::string value = entry->value(scalarType->getKind());
            CHECK(!value.empty());
            out << mStorageType->resolveToScalarType()->getVtsScalarType()
                << ": "
                << value
                << "\n";
            out.unindent();
            out << "}\n";
        }
    }

    out.unindent();
    out << "}\n";
    return OK;
}

status_t EnumType::emitVtsAttributeType(Formatter &out) const {
    out << "type: " << getVtsType() << "\n";
    out << "predefined_type: \"" << fullName() << "\"\n";
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

const Annotation *EnumType::findExportAnnotation() const {
    for (const auto &annotation : annotations()) {
        if (annotation->name() == "export") {
            return annotation;
        }
    }

    return nullptr;
}

void EnumType::appendToExportedTypesVector(
        std::vector<const Type *> *exportedTypes) const {
    if (findExportAnnotation() != nullptr) {
        exportedTypes->push_back(this);
    }
}

status_t EnumType::emitExportedHeader(Formatter &out, bool forJava) const {
    const Annotation *annotation = findExportAnnotation();
    CHECK(annotation != nullptr);

    std::string name = localName();

    const AnnotationParam *nameParam = annotation->getParam("name");
    if (nameParam != nullptr) {
        CHECK_EQ(nameParam->getValues()->size(), 1u);

        std::string quotedString = nameParam->getValues()->at(0);
        name = quotedString.substr(1, quotedString.size() - 2);
    }

    std::string valuePrefix;

    const AnnotationParam *prefixParam = annotation->getParam("value_prefix");
    if (prefixParam != nullptr) {
        CHECK_EQ(prefixParam->getValues()->size(), 1u);

        std::string quotedString = prefixParam->getValues()->at(0);
        valuePrefix = quotedString.substr(1, quotedString.size() - 2);
    }

    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != nullptr);

    if (forJava) {
        if (!name.empty()) {
            out << "public final class "
                << name
                << " {\n";

            out.indent();
        } else {
            out << "// Values declared in " << localName() << " follow.\n";
        }

        const std::string typeName =
            scalarType->getJavaType(false /* forInitializer */);

        std::vector<const EnumType *> chain;
        getTypeChain(&chain);

        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const auto &type = *it;

            for (const auto &entry : type->values()) {
                out << "public static final "
                    << typeName
                    << " "
                    << valuePrefix
                    << entry->name()
                    << " = ";

                // javaValue will make the number signed.
                std::string value = entry->javaValue(scalarType->getKind());
                CHECK(!value.empty()); // use autofilled values for java.
                out << value;

                out << ";";

                std::string comment = entry->comment();
                if (!comment.empty() && comment != value) {
                    out << " // " << comment;
                }

                out << "\n";
            }
        }

        if (!name.empty()) {
            out.unindent();
            out << "};\n";
        }
        out << "\n";

        return OK;
    }

    if (!name.empty()) {
        out << "typedef ";
    }

    out << "enum {\n";

    out.indent();

    std::vector<const EnumType *> chain;
    getTypeChain(&chain);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << valuePrefix << entry->name();

            std::string value = entry->cppValue(scalarType->getKind());
            CHECK(!value.empty()); // use autofilled values for c++.
            out << " = " << value;

            out << ",";

            std::string comment = entry->comment();
            if (!comment.empty() && comment != value) {
                out << " // " << comment;
            }

            out << "\n";
        }
    }

    out.unindent();
    out << "}";

    if (!name.empty()) {
        out << " " << name;
    }

    out << ";\n\n";

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

EnumValue::EnumValue(const char *name, ConstantExpression *value)
    : mName(name),
      mValue(value),
      mIsAutoFill(false) {
}

std::string EnumValue::name() const {
    return mName;
}

std::string EnumValue::value(ScalarType::Kind castKind) const {
    CHECK(mValue != nullptr);
    return mValue->value(castKind);
}

std::string EnumValue::cppValue(ScalarType::Kind castKind) const {
    CHECK(mValue != nullptr);
    return mValue->cppValue(castKind);
}
std::string EnumValue::javaValue(ScalarType::Kind castKind) const {
    CHECK(mValue != nullptr);
    return mValue->javaValue(castKind);
}

std::string EnumValue::comment() const {
    CHECK(mValue != nullptr);
    return mValue->description();
}

ConstantExpression *EnumValue::constExpr() const {
    CHECK(mValue != nullptr);
    return mValue;
}

void EnumValue::autofill(const EnumValue *prev, const ScalarType *type) {
    if(mValue != nullptr)
        return;
    mIsAutoFill = true;
    ConstantExpression *value = new ConstantExpression();
    if(prev == nullptr) {
        *value = ConstantExpression::Zero(type->getKind());
    } else {
        CHECK(prev->mValue != nullptr);
        *value = prev->mValue->addOne();
    }
    mValue = value;
}

bool EnumValue::isAutoFill() const {
    return mIsAutoFill;
}

bool EnumValue::isEnumValue() const {
    return true;
}

}  // namespace android

