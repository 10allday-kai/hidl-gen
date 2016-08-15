#include "EnumType.h"

#include "Formatter.h"
#include "ScalarType.h"

#include <android-base/logging.h>

namespace android {

EnumType::EnumType(
        std::vector<EnumValue *> *values, Type *storageType)
    : mValues(values),
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

std::string EnumType::getCppType(StorageMode, std::string *extra) const {
    extra->clear();

    return fullName();
}

std::string EnumType::getJavaType() const {
    return mStorageType->resolveToScalarType()->getJavaType();
}

std::string EnumType::getJavaSuffix() const {
    return mStorageType->resolveToScalarType()->getJavaSuffix();
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
    const EnumType *type = this;
    for (;;) {
        chain.push_back(type);

        const Type *superType = type->storageType();
        if (superType == NULL || !superType->isEnum()) {
            break;
        }

        type = static_cast<const EnumType *>(superType);
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << entry->name();

            const char *value = entry->value();
            if (value != NULL) {
                out << " = " << value;
            }

            out << ",\n";
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
            sprintf(out, "%lldL", (int64_t)x);
        }

        *output = out;
        return true;
    }

    return false;
}

status_t EnumType::emitJavaTypeDeclarations(Formatter &out) const {
    const ScalarType *scalarType = mStorageType->resolveToScalarType();
    CHECK(scalarType != NULL);

    out << "public final class "
        << localName()
        << " {\n";

    out.indent();

    const std::string typeName = scalarType->getJavaType();

    std::vector<const EnumType *> chain;
    const EnumType *type = this;
    for (;;) {
        chain.push_back(type);

        const Type *superType = type->storageType();
        if (superType == NULL || !superType->isEnum()) {
            break;
        }

        type = static_cast<const EnumType *>(superType);
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto &type = *it;

        for (const auto &entry : type->values()) {
            out << "public static final "
                << typeName
                << " "
                << entry->name();

            const char *value = entry->value();
            if (value != NULL) {
                out << " = ";

                std::string convertedValue;
                if (MakeSignedIntegerValue(typeName, value, &convertedValue)) {
                    out << convertedValue;
                } else {
                    // The value is not an integer, but some other string,
                    // hopefully referring to some other enum name.
                    out << value;
                }
            }

            out << ";\n";
        }
    }

    out.unindent();
    out << "};\n\n";

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

EnumValue::EnumValue(const char *name, const char *value)
    : mName(name),
      mValue(value) {
}

std::string EnumValue::name() const {
    return mName;
}

const char *EnumValue::value() const {
    return mValue;
}

}  // namespace android

