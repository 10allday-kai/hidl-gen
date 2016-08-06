#include "ScalarType.h"

#include "Formatter.h"

namespace android {

ScalarType::ScalarType(Kind kind)
    : mKind(kind) {
}

const ScalarType *ScalarType::resolveToScalarType() const {
    return this;
}

bool ScalarType::isValidEnumStorageType() const {
    // Only integer types.
    return mKind >= KIND_INT8 && mKind <= KIND_UINT64;
}

std::string ScalarType::getCppType(StorageMode, std::string *extra) const {
    static const char *const kName[] = {
        "char",
        "bool",
        "void *",
        "int8_t",
        "uint8_t",
        "int16_t",
        "uint16_t",
        "int32_t",
        "uint32_t",
        "int64_t",
        "uint64_t",
        "float",
        "double"
    };

    extra->clear();

    return kName[mKind];
}

void ScalarType::emitReaderWriter(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode) const {
    emitReaderWriterWithCast(
            out,
            name,
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode,
            false /* needsCast */);
}

void ScalarType::emitReaderWriterWithCast(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode,
        bool needsCast) const {
    static const char *const kSuffix[] = {
        "Uint8",
        "Uint8",
        "Pointer",
        "Int8",
        "Uint8",
        "Int16",
        "Uint16",
        "Int32",
        "Uint32",
        "Int64",
        "Uint64",
        "Float",
        "Double"
    };

    const std::string parcelObjDeref =
        parcelObj + (parcelObjIsPointer ? "->" : ".");

    out << "_aidl_err = "
        << parcelObjDeref
        << (isReader ? "read" : "write")
        << kSuffix[mKind]
        << "(";

    if (needsCast) {
        std::string extra;

        out << "("
            << Type::getCppType(&extra)
            << (isReader ? " *)" : ")");
    }

    if (isReader) {
        out << "&";
    }

    out << name
        << ");\n";

    handleError(out, mode);
}

}  // namespace android

