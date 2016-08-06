#include "AST.h"

#include "Coordinator.h"
#include "Formatter.h"
#include "Interface.h"
#include "Method.h"
#include "Scope.h"

#include <android-base/logging.h>
#include <sys/stat.h>
#include <vector>

namespace android {

static bool MakeParentHierarchy(const std::string &path) {
    static const mode_t kMode = 0755;

    size_t start = 1;  // Ignore leading '/'
    size_t slashPos;
    while ((slashPos = path.find("/", start)) != std::string::npos) {
        std::string partial = path.substr(0, slashPos);

        struct stat st;
        if (stat(partial.c_str(), &st) < 0) {
            if (errno != ENOENT) {
                return false;
            }

            int res = mkdir(partial.c_str(), kMode);
            if (res < 0) {
                return false;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            return false;
        }

        start = slashPos + 1;
    }

    return true;
}

static std::string upcase(const std::string in) {
    std::string out{in};

    for (auto &ch : out) {
        ch = toupper(ch);
    }

    return out;
}

status_t AST::generateCpp(const std::string &outputPath) const {
    status_t err = generateInterfaceHeader(outputPath);

    if (err == OK) {
        err = generateStubHeader(outputPath);
    }

    if (err == OK) {
        err = generateProxyHeader(outputPath);
    }

    if (err == OK) {
        err = generateAllSource(outputPath);
    }

    return err;
}

void AST::getPackageComponents(
        std::vector<std::string> *components) const {
    mPackage.getPackageComponents(components);
}

void AST::getPackageAndVersionComponents(
        std::vector<std::string> *components, bool cpp_compatible) const {
    mPackage.getPackageAndVersionComponents(components, cpp_compatible);
}

std::string AST::makeHeaderGuard(const std::string &baseName) const {
    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, true /* cpp_compatible */);

    std::string guard = "HIDL_GENERATED";
    for (const auto &component : packageComponents) {
        guard += "_";
        guard += component;
    }

    guard += "_";
    guard += baseName;
    guard += "_H_";

    return guard;
}

void AST::enterLeaveNamespace(Formatter &out, bool enter) const {
    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, true /* cpp_compatible */);

    if (enter) {
        for (const auto &component : packageComponents) {
            out << "namespace " << component << " {\n";
        }

        out.setNamespace(mPackage.cppNamespace());
    } else {
        out.setNamespace(std::string());

        for (auto it = packageComponents.rbegin();
                it != packageComponents.rend();
                ++it) {
            out << "}  // namespace " << *it << "\n";
        }
    }
}

status_t AST::generateInterfaceHeader(const std::string &outputPath) const {
    const std::string packagePath =
        mCoordinator->getPackagePath(mPackage, true /* relative */);

    std::string path = outputPath;
    path.append("android/hardware/");
    path.append(packagePath);

    std::string ifaceName;
    bool isInterface = true;
    if (!AST::isInterface(&ifaceName)) {
        ifaceName = "types";
        isInterface = false;
    }
    path.append(ifaceName);
    path.append(".h");

    CHECK(MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard(ifaceName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    for (const auto &item : mImportedNames) {
        out << "#include <";

        std::vector<std::string> components;
        item.getPackageAndVersionComponents(
                &components, false /* cpp_compatible */);

        for (const auto &component : components) {
            out << component << "/";
        }

        out << item.name()
            << ".h>\n";
    }

    if (!mImportedNames.empty()) {
        out << "\n";
    }

    out << "#include <hwbinder/HidlSupport.h>\n";

    if (isInterface) {
        out << "#include <hwbinder/IBinder.h>\n";
        out << "#include <hwbinder/IInterface.h>\n";
        out << "#include <hwbinder/Status.h>\n";
    }

    out << "#include <utils/NativeHandle.h>\n\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    if (isInterface) {
        out << "struct "
            << ifaceName
            << " : public ";

        const Interface *iface = mRootScope->getInterface();
        const Interface *superType = iface->superType();

        if (superType != NULL) {
            out << superType->fullName();
        } else {
            out << "::android::hardware::IInterface";
        }

        out << " {\n";

        out.indent();

        // cut off the leading 'I'.
        const std::string baseName = ifaceName.substr(1);

        out << "DECLARE_HWBINDER_META_INTERFACE(" << baseName << ");\n\n";
    }

    status_t err = emitTypeDeclarations(out);

    if (err != OK) {
        return err;
    }

    if (isInterface) {
        const Interface *iface = mRootScope->getInterface();
        const Interface *superType = iface->superType();

        out << "enum Call {\n";
        out.indent();

        bool first = true;
        for (const auto &method : iface->methods()) {
            out << upcase(method->name());

            if (first) {
                out << " = ";
                if (superType != NULL) {
                    out << superType->fullName()
                        << "::Call::CallCount";
                } else {
                    out << "::android::hardware::IBinder::FIRST_CALL_TRANSACTION";
                }

                first = false;
            }

            out << ",\n";
        }

        out << "CallCount\n";

        out.unindent();
        out << "};\n\n";

        bool haveCallbacks = false;
        for (const auto &method : iface->methods()) {
            const bool returnsValue = !method->results().empty();

            if (!returnsValue) {
                continue;
            }

            haveCallbacks = true;

            out << "using "
                << method->name()
                << "_cb = std::function<void("
                << Method::GetSignature(method->results())
                << ")>;\n";
        }

        if (haveCallbacks) {
            out << "\n";
        }

        for (const auto &method : iface->methods()) {
            const bool returnsValue = !method->results().empty();

            out << "virtual ::android::hardware::Status "
                << method->name()
                << "("
                << Method::GetSignature(method->args());

            if (returnsValue) {
                if (!method->args().empty()) {
                    out << ", ";
                }

                out << method->name() << "_cb _aidl_cb = nullptr";
            }

            out << ") = 0;\n";
        }
    }

    if (isInterface) {
        out.unindent();

        out << "};\n";
    }

    out << "\n";
    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::emitTypeDeclarations(Formatter &out) const {
    return mRootScope->emitTypeDeclarations(out);
}

status_t AST::generateStubHeader(const std::string &outputPath) const {
    std::string ifaceName;
    if (!AST::isInterface(&ifaceName)) {
        // types.hal does not get a stub header.
        return OK;
    }

    const std::string packagePath =
        mCoordinator->getPackagePath(mPackage, true /* relative */);

    // cut off the leading 'I'.
    const std::string baseName = ifaceName.substr(1);

    std::string path = outputPath;
    path.append("android/hardware/");
    path.append(packagePath);
    path.append("Bn");
    path.append(baseName);
    path.append(".h");

    CHECK(MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard("Bn" + baseName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, false /* cpp_compatible */);

    out << "#include <";
    for (const auto &component : packageComponents) {
        out << component << "/";
    }
    out << ifaceName << ".h>\n\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    out << "struct "
        << "Bn"
        << baseName
        << " : public ::android::hardware::BnInterface<"
        << ifaceName
        << "> {\n";

    out.indent();

    out << "::android::status_t onTransact(\n";
    out.indent();
    out.indent();
    out << "uint32_t _aidl_code,\n";
    out << "const ::android::hardware::Parcel &_aidl_data,\n";
    out << "::android::hardware::Parcel *_aidl_reply,\n";
    out << "uint32_t _aidl_flags = 0,\n";
    out << "TransactCallback _aidl_cb = nullptr) override;\n";
    out.unindent();
    out.unindent();

    out.unindent();

    out << "};\n\n";

    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::generateProxyHeader(const std::string &outputPath) const {
    std::string ifaceName;
    if (!AST::isInterface(&ifaceName)) {
        // types.hal does not get a proxy header.
        return OK;
    }

    const std::string packagePath =
        mCoordinator->getPackagePath(mPackage, true /* relative */);

    // cut off the leading 'I'.
    const std::string baseName = ifaceName.substr(1);

    std::string path = outputPath;
    path.append("android/hardware/");
    path.append(packagePath);
    path.append("Bp");
    path.append(baseName);
    path.append(".h");

    CHECK(MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard("Bp" + baseName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, false /* cpp_compatible */);

    out << "#include <";
    for (const auto &component : packageComponents) {
        out << component << "/";
    }
    out << ifaceName << ".h>\n\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    out << "struct "
        << "Bp"
        << baseName
        << " : public ::android::hardware::BpInterface<"
        << ifaceName
        << "> {\n";

    out.indent();

    out << "explicit Bp"
        << baseName
        << "(const ::android::sp<::android::hardware::IBinder> &_aidl_impl);"
        << "\n\n";

    const Interface *iface = mRootScope->getInterface();

    std::vector<const Interface *> chain;
    while (iface != NULL) {
        chain.push_back(iface);
        iface = iface->superType();
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const Interface *superInterface = *it;

        out << "// Methods from "
            << superInterface->fullName()
            << " follow.\n";

        for (const auto &method : superInterface->methods()) {
            const bool returnsValue = !method->results().empty();

            out << "::android::hardware::Status "
                << method->name()
                << "("
                << Method::GetSignature(method->args());

            if (returnsValue) {
                if (!method->args().empty()) {
                    out << ", ";
                }

                out << method->name() << "_cb _aidl_cb";
            }

            out << ") override;\n";
        }

        out << "\n";
    }

    out.unindent();

    out << "};\n\n";

    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::generateAllSource(const std::string &outputPath) const {
    const std::string packagePath =
        mCoordinator->getPackagePath(mPackage, true /* relative */);

    std::string path = outputPath;
    path.append("android/hardware/");
    path.append(packagePath);

    std::string ifaceName;
    std::string baseName;

    bool isInterface = true;
    if (!AST::isInterface(&ifaceName)) {
        baseName = "types";
        isInterface = false;
    } else {
        baseName = ifaceName.substr(1); // cut off the leading 'I'.
    }

    path.append(baseName);

    if (baseName != "types") {
        path.append("All");
    }

    path.append(".cpp");

    CHECK(MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, false /* cpp_compatible */);

    std::string prefix;
    for (const auto &component : packageComponents) {
        prefix += component;
        prefix += "/";
    }

    if (isInterface) {
        out << "#include <" << prefix << "/Bp" << baseName << ".h>\n";
        out << "#include <" << prefix << "/Bn" << baseName << ".h>\n";
    } else {
        out << "#include <" << prefix << "types.h>\n";
    }

    out << "\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    status_t err = generateTypeSource(out, ifaceName);

    if (err == OK && isInterface) {
        err = generateProxySource(out, baseName);
    }

    if (err == OK && isInterface) {
        err = generateStubSource(out, baseName);
    }

    enterLeaveNamespace(out, false /* enter */);

    return err;
}

status_t AST::generateTypeSource(
        Formatter &out, const std::string &ifaceName) const {
    return mRootScope->emitTypeDefinitions(out, ifaceName);
}

void AST::emitCppReaderWriter(
        Formatter &out,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        const TypedVar *arg,
        bool isReader,
        Type::ErrorMode mode) const {
    const Type &type = arg->type();

    if (isReader) {
        std::string extra;
        out << type.getCppResultType(&extra)
            << " "
            << arg->name()
            << extra
            << ";\n";
    }

    type.emitReaderWriter(
            out,
            arg->name(),
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode);
}

status_t AST::generateProxySource(
        Formatter &out, const std::string &baseName) const {
    const std::string klassName = "Bp" + baseName;

    out << klassName
        << "::"
        << klassName
        << "(const ::android::sp<::android::hardware::IBinder> &_aidl_impl)\n";

    out.indent();
    out.indent();

    out << ": BpInterface"
        << "<I"
        << baseName
        << ">(_aidl_impl) {\n";

    out.unindent();
    out.unindent();
    out << "}\n\n";

    const Interface *iface = mRootScope->getInterface();

    std::vector<const Interface *> chain;
    while (iface != NULL) {
        chain.push_back(iface);
        iface = iface->superType();
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const Interface *superInterface = *it;

        for (const auto &method : superInterface->methods()) {
            const bool returnsValue = !method->results().empty();

            out << "::android::hardware::Status "
                << klassName
                << "::"
                << method->name()
                << "("
                << Method::GetSignature(method->args());

            if (returnsValue) {
                if (!method->args().empty()) {
                    out << ", ";
                }

                out << method->name() << "_cb _aidl_cb";
            }

            out << ") {\n";

            out.indent();

            out << "::android::hardware::Parcel _aidl_data;\n";
            out << "::android::hardware::Parcel _aidl_reply;\n";
            out << "::android::status_t _aidl_err;\n\n";
            out << "::android::hardware::Status _aidl_status;\n";

            out << "_aidl_err = _aidl_data.writeInterfaceToken("
                << superInterface->fullName()
                << "::getInterfaceDescriptor());\n";

            out << "if (_aidl_err != ::android::OK) { goto _aidl_error; }\n\n";

            for (const auto &arg : method->args()) {
                emitCppReaderWriter(
                        out,
                        "_aidl_data",
                        false /* parcelObjIsPointer */,
                        arg,
                        false /* reader */,
                        Type::ErrorMode_Goto);
            }

            out << "_aidl_err = remote()->transact(I"
                      << baseName
                      << "::"
                      << upcase(method->name())
                      << ", _aidl_data, &_aidl_reply);\n";

            out << "if (_aidl_err != ::android::OK) { goto _aidl_error; }\n\n";

            out << "_aidl_err = _aidl_status.readFromParcel(_aidl_reply);\n";
            out << "if (_aidl_err != ::android::OK) { goto _aidl_error; }\n\n";

            out << "if (!_aidl_status.isOk()) { return _aidl_status; }\n\n";

            for (const auto &arg : method->results()) {
                emitCppReaderWriter(
                        out,
                        "_aidl_reply",
                        false /* parcelObjIsPointer */,
                        arg,
                        true /* reader */,
                        Type::ErrorMode_Goto);
            }

            if (returnsValue) {
                out << "if (_aidl_cb != nullptr) {\n";
                out.indent();
                out << "_aidl_cb(";

                bool first = true;
                for (const auto &arg : method->results()) {
                    if (!first) {
                        out << ", ";
                    }

                    if (arg->type().resultNeedsDeref()) {
                        out << "*";
                    }
                    out << arg->name();

                    first = false;
                }

                out << ");\n";
                out.unindent();
                out << "}\n\n";
            }

            out.unindent();
            out << "_aidl_error:\n";
            out.indent();
            out << "_aidl_status.setFromStatusT(_aidl_err);\n"
                      << "return _aidl_status;\n";

            out.unindent();
            out << "}\n\n";
        }
    }

    return OK;
}

status_t AST::generateStubSource(
        Formatter &out, const std::string &baseName) const {
    out << "IMPLEMENT_HWBINDER_META_INTERFACE("
        << baseName
        << ", \""
        << mPackage.string()
        << "::I"
        << baseName
        << "\");\n\n";

    const std::string klassName = "Bn" + baseName;

    out << "::android::status_t " << klassName << "::onTransact(\n";

    out.indent();
    out.indent();

    out << "uint32_t _aidl_code,\n"
        << "const ::android::hardware::Parcel &_aidl_data,\n"
        << "::android::hardware::Parcel *_aidl_reply,\n"
        << "uint32_t _aidl_flags,\n"
        << "TransactCallback _aidl_cb) {\n";

    out.unindent();

    out << "::android::status_t _aidl_err = ::android::OK;\n\n";

    out << "switch (_aidl_code) {\n";
    out.indent();

    const Interface *iface = mRootScope->getInterface();

    std::vector<const Interface *> chain;
    while (iface != NULL) {
        chain.push_back(iface);
        iface = iface->superType();
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const Interface *superInterface = *it;

        for (const auto &method : superInterface->methods()) {
            out << "case "
                << superInterface->fullName()
                << "::Call::"
                << upcase(method->name())
                << ":\n{\n";

            out.indent();

            status_t err =
                generateStubSourceForMethod(out, superInterface, method);

            if (err != OK) {
                return err;
            }

            out.unindent();
            out << "}\n\n";
        }
    }

    out << "default:\n{\n";
    out.indent();

    out << "return ::android::hardware::BnInterface<I"
        << baseName
        << ">::onTransact(\n";

    out.indent();
    out.indent();

    out << "_aidl_code, _aidl_data, _aidl_reply, "
        << "_aidl_flags, _aidl_cb);\n";

    out.unindent();
    out.unindent();

    out.unindent();
    out << "}\n";

    out.unindent();
    out << "}\n\n";

    out << "if (_aidl_err == ::android::UNEXPECTED_NULL) {\n";
    out.indent();
    out << "_aidl_err = ::android::hardware::Status::fromExceptionCode(\n";
    out.indent();
    out.indent();
    out << "::android::hardware::Status::EX_NULL_POINTER)\n";
    out.indent();
    out.indent();
    out << ".writeToParcel(_aidl_reply);\n";
    out.unindent();
    out.unindent();
    out.unindent();
    out.unindent();

    out.unindent();
    out << "}\n\n";

    out << "return _aidl_err;\n";

    out.unindent();
    out << "}\n\n";

    return OK;
}

status_t AST::generateStubSourceForMethod(
        Formatter &out, const Interface *iface, const Method *method) const {
    out << "if (!_aidl_data.enforceInterface("
        << iface->fullName()
        << "::getInterfaceDescriptor())) {\n";

    out.indent();
    out << "_aidl_err = ::android::BAD_TYPE;\n";
    out << "break;\n";
    out.unindent();
    out << "}\n\n";

    for (const auto &arg : method->args()) {
        emitCppReaderWriter(
                out,
                "_aidl_data",
                false /* parcelObjIsPointer */,
                arg,
                true /* reader */,
                Type::ErrorMode_Break);
    }

    const bool returnsValue = !method->results().empty();

    if (returnsValue) {
        out << "bool _aidl_callbackCalled = false;\n\n";
    }

    out << "::android::hardware::Status _aidl_status(\n";
    out.indent();
    out.indent();
    out << method->name() << "(";

    bool first = true;
    for (const auto &arg : method->args()) {
        if (!first) {
            out << ", ";
        }

        if (arg->type().resultNeedsDeref()) {
            out << "*";
        }

        out << arg->name();

        first = false;
    }

    if (returnsValue) {
        if (!first) {
            out << ", ";
        }

        out << "[&](";

        first = true;
        for (const auto &arg : method->results()) {
            if (!first) {
                out << ", ";
            }

            out << "const auto &" << arg->name();

            first = false;
        }

        out << ") {\n";
        out.indent();
        out << "_aidl_callbackCalled = true;\n\n";

        out << "::android::hardware::Status::ok()"
                  << ".writeToParcel(_aidl_reply);\n\n";

        for (const auto &arg : method->results()) {
            emitCppReaderWriter(
                    out,
                    "_aidl_reply",
                    true /* parcelObjIsPointer */,
                    arg,
                    false /* reader */,
                    Type::ErrorMode_Ignore);
        }

        out << "_aidl_cb(*_aidl_reply);\n";

        out.unindent();
        out << "}\n";
    }

    out.unindent();
    out.unindent();
    out << "));\n\n";

    if (returnsValue) {
        out << "if (!_aidl_callbackCalled) {\n";
        out.indent();
    }

    out << "_aidl_err = _aidl_status.writeToParcel(_aidl_reply);\n";

    if (returnsValue) {
        out.unindent();
        out << "}\n\n";
    }

    out << "break;\n";

    return OK;
}

}  // namespace android

