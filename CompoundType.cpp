#include "CompoundType.h"

#include "Formatter.h"

#include <android-base/logging.h>

namespace android {

CompoundType::CompoundType(Style style)
    : mStyle(style),
      mFields(NULL) {
}

bool CompoundType::setFields(std::vector<CompoundField *> *fields) {
    mFields = fields;

    for (const auto &field : *fields) {
        const Type &type = field->type();

        if (mStyle == STYLE_UNION) {
            if (type.needsEmbeddedReadWrite()) {
                // Can't have those in a union.

                fprintf(stderr,
                        "Unions must not contain any types that need fixup.\n");

                return false;
            }
        } else {
            CHECK_EQ(mStyle, STYLE_STRUCT);

            if (type.isInterface()) {
                fprintf(stderr,
                        "Structs must not contain references to interfaces.\n");

                return false;
            }
        }
    }

    return true;
}

std::string CompoundType::getCppType(
        StorageMode mode, std::string *extra) const {
    extra->clear();
    const std::string base = fullName();

    switch (mode) {
        case StorageMode_Stack:
            return base;

        case StorageMode_Argument:
            return "const " + base + "&";

        case StorageMode_Result:
            return "const " + base + "*";
    }
}

void CompoundType::emitReaderWriter(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode) const {
    const std::string parentName = "_aidl_" + name + "_parent";

    out << "size_t " << parentName << ";\n\n";

    const std::string parcelObjDeref =
        parcelObj + (parcelObjIsPointer ? "->" : ".");

    if (isReader) {
        out << name
            << " = (const "
            << fullName()
            << " *)"
            << parcelObjDeref
            << "readBuffer("
            << "&"
            << parentName
            << ");\n";

        out << "if ("
            << name
            << " == nullptr) {\n";

        out.indent();

        out << "_aidl_err = ::android::UNKNOWN_ERROR;\n";
        handleError2(out, mode);

        out.unindent();
        out << "}\n\n";
    } else {
        out << "_aidl_err = "
            << parcelObjDeref
            << "writeBuffer(&"
            << name
            << ", sizeof("
            << name
            << "), &"
            << parentName
            << ");\n";

        handleError(out, mode);
    }

    if (mStyle != STYLE_STRUCT || !needsEmbeddedReadWrite()) {
        return;
    }

    emitReaderWriterEmbedded(
            out,
            name,
            isReader /* nameIsPointer */,
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode,
            parentName,
            "0 /* parentOffset */");
}

void CompoundType::emitReaderWriterEmbedded(
        Formatter &out,
        const std::string &name,
        bool nameIsPointer,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode,
        const std::string &parentName,
        const std::string &offsetText) const {
    emitReaderWriterEmbeddedForTypeName(
            out,
            name,
            nameIsPointer,
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode,
            parentName,
            offsetText,
            fullName(),
            "" /* childName */);
}

status_t CompoundType::emitTypeDeclarations(Formatter &out) const {
    out << ((mStyle == STYLE_STRUCT) ? "struct" : "union")
        << " "
        << localName()
        << " {\n";

    out.indent();

    Scope::emitTypeDeclarations(out);

    for (const auto &field : *mFields) {
        std::string extra;
        out << field->type().getCppType(&extra)
            << " "
            << field->name()
            << extra
            << ";\n";
    }

    if (needsEmbeddedReadWrite()) {
        out << "\n::android::status_t readEmbeddedFromParcel(\n";

        out.indent();
        out.indent();

        out << "const ::android::hardware::Parcel &parcel,\n"
                  << "size_t parentHandle,\n"
                  << "size_t parentOffset);\n\n";

        out.unindent();
        out.unindent();

        out << "::android::status_t writeEmbeddedToParcel(\n";

        out.indent();
        out.indent();

        out << "::android::hardware::Parcel *parcel,\n"
                  << "size_t parentHandle,\n"
                  << "size_t parentOffset) const;\n";

        out.unindent();
        out.unindent();
    }

    out.unindent();
    out << "};\n\n";

    return OK;
}

status_t CompoundType::emitTypeDefinitions(
        Formatter &out, const std::string prefix) const {
    status_t err = Scope::emitTypeDefinitions(out, prefix + "::" + localName());

    if (err != OK) {
        return err;
    }

    if (!needsEmbeddedReadWrite()) {
        return OK;
    }

    emitStructReaderWriter(out, prefix, true /* isReader */);
    emitStructReaderWriter(out, prefix, false /* isReader */);

    return OK;
}

void CompoundType::emitStructReaderWriter(
        Formatter &out, const std::string &prefix, bool isReader) const {
    out << "::android::status_t "
              << (prefix.empty() ? "" : (prefix + "::"))
              << localName()
              << (isReader ? "::readEmbeddedFromParcel"
                           : "::writeEmbeddedToParcel")
              << "(\n";

    out.indent();
    out.indent();

    if (isReader) {
        out << "const ::android::hardware::Parcel &parcel,\n";
    } else {
        out << "::android::hardware::Parcel *parcel,\n";
    }

    out << "size_t parentHandle,\n"
        << "size_t parentOffset)";

    if (!isReader) {
        out << " const";
    }

    out << " {\n";

    out.unindent();
    out.unindent();
    out.indent();

    out << "::android::status_t _aidl_err = ::android::OK;\n\n";

    for (const auto &field : *mFields) {
        if (!field->type().needsEmbeddedReadWrite()) {
            continue;
        }

        field->type().emitReaderWriterEmbedded(
                out,
                field->name(),
                false /* nameIsPointer */,
                "parcel",
                !isReader /* parcelObjIsPointer */,
                isReader,
                ErrorMode_Return,
                "parentHandle",
                "parentOffset + offsetof("
                    + fullName()
                    + ", "
                    + field->name()
                    + ")");
    }

    out.unindent();
    out << "_aidl_error:\n";
    out.indent();
    out << "return _aidl_err;\n";

    out.unindent();
    out << "}\n\n";
}

bool CompoundType::needsEmbeddedReadWrite() const {
    if (mStyle != STYLE_STRUCT) {
        return false;
    }

    for (const auto &field : *mFields) {
        if (field->type().needsEmbeddedReadWrite()) {
            return true;
        }
    }

    return false;
}

bool CompoundType::resultNeedsDeref() const {
    return true;
}

////////////////////////////////////////////////////////////////////////////////

CompoundField::CompoundField(const char *name, Type *type)
    : mName(name),
      mType(type) {
}

std::string CompoundField::name() const {
    return mName;
}

const Type &CompoundField::type() const {
    return *mType;
}

}  // namespace android

