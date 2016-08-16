#include "HandleType.h"

#include "Formatter.h"

#include <android-base/logging.h>

namespace android {

HandleType::HandleType() {}

std::string HandleType::getCppType(StorageMode, std::string *extra) const {
    extra->clear();

    return "const ::native_handle_t*";
}

std::string HandleType::getJavaType() const {
    CHECK(!"Should not be here");
    return std::string();
}

void HandleType::emitReaderWriter(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode) const {
    const std::string parcelObjDeref =
        parcelObj + (parcelObjIsPointer ? "->" : ".");

    if (isReader) {
        out << name
            << " = "
            << parcelObjDeref
            << "readNativeHandleNoDup();\n\n";

        out << "if ("
            << name
            << " == nullptr) {\n";

        out.indent();

        out << "_hidl_err = ::android::UNKNOWN_ERROR;\n";
        handleError2(out, mode);

        out.unindent();
        out << "}\n\n";
    } else {
        out << "_hidl_err = ";
        out << parcelObjDeref
            << "writeNativeHandleNoDup("
            << name
            << ");\n";

        handleError(out, mode);
    }
}

void HandleType::emitReaderWriterEmbedded(
        Formatter &out,
        const std::string &name,
        bool nameIsPointer,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode,
        const std::string &parentName,
        const std::string &offsetText) const {
    if (isReader) {
        const std::string ptrName = "_hidl_" + name  + "_ptr";

        out << "const native_handle_t *"
            << ptrName
            << " = "
            << parcelObj
            << (parcelObjIsPointer ? "->" : ".")
            << "readEmbeddedNativeHandle(\n";

        out.indent();
        out.indent();

        out << parentName
            << ",\n"
            << offsetText
            << ");\n\n";

        out.unindent();
        out.unindent();

        out << "if ("
            << ptrName
            << " == nullptr) {\n";

        out.indent();
        out << "_hidl_err = ::android::UNKNOWN_ERROR;\n";
        handleError2(out, mode);
        out.unindent();
        out << "}\n\n";
    } else {
        out << "_hidl_err = "
            << parcelObj
            << (parcelObjIsPointer ? "->" : ".")
            << "writeEmbeddedNativeHandle(\n";

        out.indent();
        out.indent();

        out << (nameIsPointer ? ("*" + name) : name)
            << ",\n"
            << parentName
            << ",\n"
            << offsetText
            << ");\n\n";

        out.unindent();
        out.unindent();

        handleError(out, mode);
    }
}

bool HandleType::needsEmbeddedReadWrite() const {
    return true;
}

bool HandleType::isJavaCompatible() const {
    return false;
}

}  // namespace android

