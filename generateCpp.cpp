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

#include "AST.h"

#include "Coordinator.h"
#include "EnumType.h"
#include "Interface.h"
#include "Method.h"
#include "ScalarType.h"
#include "Scope.h"

#include <algorithm>
#include <hidl-util/Formatter.h>
#include <hidl-util/StringHelper.h>
#include <android-base/logging.h>
#include <string>
#include <vector>

namespace android {

status_t AST::generateCpp(const std::string &outputPath) const {
    status_t err = generateInterfaceHeader(outputPath);

    if (err == OK) {
        err = generateStubHeader(outputPath);
    }

    if (err == OK) {
        err = generateHwBinderHeader(outputPath);
    }

    if (err == OK) {
        err = generateProxyHeader(outputPath);
    }

    if (err == OK) {
        err = generateAllSource(outputPath);
    }

    if (err == OK) {
        generatePassthroughHeader(outputPath);
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

std::string AST::makeHeaderGuard(const std::string &baseName,
                                 bool indicateGenerated) const {
    std::string guard;

    if (indicateGenerated) {
        guard += "HIDL_GENERATED_";
    }

    guard += StringHelper::Uppercase(mPackage.tokenName());
    guard += "_";
    guard += StringHelper::Uppercase(baseName);
    guard += "_H";

    return guard;
}

// static
void AST::generateCppPackageInclude(
        Formatter &out,
        const FQName &package,
        const std::string &klass) {

    out << "#include <";

    std::vector<std::string> components;
    package.getPackageAndVersionComponents(&components, false /* cpp_compatible */);

    for (const auto &component : components) {
        out << component << "/";
    }

    out << klass
        << ".h>\n";
}

void AST::enterLeaveNamespace(Formatter &out, bool enter) const {
    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, true /* cpp_compatible */);

    if (enter) {
        for (const auto &component : packageComponents) {
            out << "namespace " << component << " {\n";
        }

        out.setNamespace(mPackage.cppNamespace() + "::");
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

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));

    std::string ifaceName;
    bool isInterface = true;
    if (!AST::isInterface(&ifaceName)) {
        ifaceName = "types";
        isInterface = false;
    }
    path.append(ifaceName);
    path.append(".h");

    CHECK(Coordinator::MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard(ifaceName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    for (const auto &item : mImportedNames) {
        generateCppPackageInclude(out, item, item.name());
    }

    if (!mImportedNames.empty()) {
        out << "\n";
    }

    if (isInterface) {
        if (isIBase()) {
            out << "// skipped #include IServiceNotification.h\n\n";
        } else {
            out << "#include <android/hidl/manager/1.0/IServiceNotification.h>\n\n";
        }
    }

    out << "#include <hidl/HidlSupport.h>\n";
    out << "#include <hidl/MQDescriptor.h>\n";

    if (isInterface) {
        out << "#include <hidl/ServiceManagement.h>\n";
        out << "#include <hidl/Status.h>\n";
    }

    out << "#include <utils/NativeHandle.h>\n";
    out << "#include <utils/misc.h>\n\n"; /* for report_sysprop_change() */

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    if (isInterface) {
        out << "struct "
            << ifaceName;

        const Interface *iface = mRootScope->getInterface();
        const Interface *superType = iface->superType();

        if (superType == NULL) {
            out << " : virtual public ::android::RefBase";
        } else {
            out << " : public "
                << superType->fullName();
        }

        out << " {\n";

        out.indent();

    }

    status_t err = emitTypeDeclarations(out);

    if (err != OK) {
        return err;
    }

    if (isInterface) {
        const Interface *iface = mRootScope->getInterface();
        const Interface *superType = iface->superType();
        const std::string baseName = iface->getBaseName();
        out << "constexpr static ::android::hardware::hidl_version version = {"
            << mPackage.getPackageMajorVersion() << ","
            << mPackage.getPackageMinorVersion() << "};\n";
        out << "virtual const ::android::hardware::hidl_version&"
            << "getInterfaceVersion() const {\n";
        out.indent();
        out << "return version;\n";
        out.unindent();
        out << "}\n\n";
        out << "virtual bool isRemote() const ";
        if (!isIBase()) {
            out << "override ";
        }
        out << "{ return false; }\n\n";

        for (const auto &method : iface->methods()) {
            out << "\n";

            const bool returnsValue = !method->results().empty();
            const TypedVar *elidedReturn = method->canElideCallback();

            if (elidedReturn == nullptr && returnsValue) {
                out << "using "
                    << method->name()
                    << "_cb = std::function<void("
                    << Method::GetArgSignature(method->results(),
                                               true /* specify namespaces */)
                    << ")>;\n";
            }

            method->dumpAnnotations(out);

            if (elidedReturn) {
                out << "virtual ::android::hardware::Return<";
                out << elidedReturn->type().getCppResultType() << "> ";
            } else {
                out << "virtual ::android::hardware::Return<void> ";
            }

            out << method->name()
                << "("
                << Method::GetArgSignature(method->args(),
                                           true /* specify namespaces */);

            if (returnsValue && elidedReturn == nullptr) {
                if (!method->args().empty()) {
                    out << ", ";
                }

                out << method->name() << "_cb _hidl_cb";
            }

            out << ")";
            if (method->isHidlReserved()) {
                if (!isIBase()) {
                    out << " override";
                }
                out << " {\n";
                out.indent();
                method->cppImpl(out);
                out.unindent();
                out << "\n}\n";
            } else {
                out << " = 0;\n";
            }
        }

        if (!iface->isRootType()) {
            out << "// cast static functions\n";
            std::string childTypeResult = iface->getCppResultType();

            for (const Interface *superType : iface->superTypeChain()) {
                out << "static "
                    << childTypeResult
                    << " castFrom("
                    << superType->getCppArgumentType()
                    << " parent"
                    << ");\n";
            }
        }

        out << "\nstatic const char* descriptor;\n\n";

        if (isIBase()) {
            out << "// skipped DECLARE_SERVICE_MANAGER_INTERACTIONS\n\n";
        } else {
            out << "DECLARE_SERVICE_MANAGER_INTERACTIONS(" << baseName << ")\n\n";
        }

        out << "private: static int hidlStaticBlock;\n";
    }

    if (isInterface) {
        out.unindent();

        out << "};\n\n";
    }

    err = mRootScope->emitGlobalTypeDeclarations(out);

    if (err != OK) {
        return err;
    }

    out << "\n";
    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::generateHwBinderHeader(const std::string &outputPath) const {
    std::string ifaceName;
    bool isInterface = AST::isInterface(&ifaceName);
    const Interface *iface = nullptr;
    std::string baseName{};
    std::string klassName{};

    if(isInterface) {
        iface = mRootScope->getInterface();
        baseName = iface->getBaseName();
        klassName = "IHw" + baseName;
    } else {
        klassName = "hwtypes";
    }

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));
    path.append(klassName + ".h");

    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard(klassName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    if (isInterface) {
        generateCppPackageInclude(out, mPackage, ifaceName);
    } else {
        generateCppPackageInclude(out, mPackage, "types");
    }

    out << "\n";

    for (const auto &item : mImportedNames) {
        if (item.name() == "types") {
            generateCppPackageInclude(out, item, "hwtypes");
        } else {
            generateCppPackageInclude(out, item, "Bn" + item.getInterfaceBaseName());
        }
    }

    out << "\n";

    out << "#include <hidl/HidlTransportSupport.h>\n";
    out << "#include <hidl/Status.h>\n";
    out << "#include <hwbinder/IBinder.h>\n";
    out << "#include <hwbinder/IInterface.h>\n";

    out << "\n";

    enterLeaveNamespace(out, true /* enter */);

    if (isInterface) {
        out << "\n";

        out << "struct "
            << klassName
            << " : public "
            << ifaceName;

        const Interface *superType = iface->superType();

        out << ", public ::android::hardware::IInterface";

        out << " {\n";

        out.indent();

        out << "DECLARE_HWBINDER_META_INTERFACE(" << baseName << ");\n\n";

        out.unindent();

        out << "};\n\n";
    }

    status_t err = mRootScope->emitGlobalHwDeclarations(out);
    if (err != OK) {
        return err;
    }

    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::emitTypeDeclarations(Formatter &out) const {
    return mRootScope->emitTypeDeclarations(out);
}

status_t AST::generateStubMethod(Formatter &out,
                                 const Method *method) const {
    out << "inline ";

    method->generateCppSignature(out);

    const bool returnsValue = !method->results().empty();
    const TypedVar *elidedReturn = method->canElideCallback();
    out << " {\n";
    out.indent();
    out << "return mImpl->"
        << method->name()
        << "(";
    bool first = true;
    for (const auto &arg : method->args()) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << arg->name();
    }
    if (returnsValue && elidedReturn == nullptr) {
        if (!method->args().empty()) {
            out << ", ";
        }

        out << "_hidl_cb";
    }
    out << ");\n";
    out.unindent();
    out << "}";

    out << ";\n";

    return OK;
}

status_t AST::generatePassthroughMethod(Formatter &out,
                                        const Method *method) const {
    method->generateCppSignature(out);

    out << " {\n";
    out.indent();

    const bool returnsValue = !method->results().empty();
    const TypedVar *elidedReturn = method->canElideCallback();

    if (returnsValue && elidedReturn == nullptr) {
        generateCheckNonNull(out, "_hidl_cb");
    }

    generateCppInstrumentationCall(
            out,
            InstrumentationEvent::PASSTHROUGH_ENTRY,
            method);

    out << "auto _hidl_return = ";

    if (method->isOneway()) {
        out << "addOnewayTask([this";
        for (const auto &arg : method->args()) {
            out << ", " << arg->name();
        }
        out << "] {\n";
        out.indent();
        out << "this->";
    }

    out << "mImpl->"
        << method->name()
        << "(";

    bool first = true;
    for (const auto &arg : method->args()) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << arg->name();
    }
    if (returnsValue && elidedReturn == nullptr) {
        if (!method->args().empty()) {
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
        status_t status = generateCppInstrumentationCall(
                out,
                InstrumentationEvent::PASSTHROUGH_EXIT,
                method);
        if (status != OK) {
            return status;
        }

        out << "_hidl_cb(";
        first = true;
        for (const auto &arg : method->results()) {
            if (!first) {
                out << ", ";
            }

            out << arg->name();

            first = false;
        }
        out << ");\n";
        out.unindent();
        out << "});\n\n";
    } else {
        out << ");\n\n";
        if (elidedReturn != nullptr) {
            out << elidedReturn->type().getCppResultType()
                << " "
                << elidedReturn->name()
                << " = _hidl_return;\n";
        }
        status_t status = generateCppInstrumentationCall(
                out,
                InstrumentationEvent::PASSTHROUGH_EXIT,
                method);
        if (status != OK) {
            return status;
        }
    }

    if (method->isOneway()) {
        out.unindent();
        out << "});\n";
    }

    out << "return _hidl_return;\n";

    out.unindent();
    out << "}\n";

    return OK;
}

status_t AST::generateMethods(Formatter &out, MethodGenerator gen) const {

    const Interface *iface = mRootScope->getInterface();

    const Interface *prevIterface = nullptr;
    for (const auto &tuple : iface->allMethodsFromRoot()) {
        const Method *method = tuple.method();
        const Interface *superInterface = tuple.interface();

        if(prevIterface != superInterface) {
            if (prevIterface != nullptr) {
                out << "\n";
            }
            out << "// Methods from "
                << superInterface->fullName()
                << " follow.\n";
            prevIterface = superInterface;
        }
        status_t err = gen(method, superInterface);

        if (err != OK) {
            return err;
        }
    }

    out << "\n";

    return OK;
}

status_t AST::generateStubHeader(const std::string &outputPath) const {
    std::string ifaceName;
    if (!AST::isInterface(&ifaceName)) {
        // types.hal does not get a stub header.
        return OK;
    }

    const Interface *iface = mRootScope->getInterface();
    const std::string baseName = iface->getBaseName();
    const std::string klassName = "Bn" + baseName;

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));
    path.append(klassName);
    path.append(".h");

    CHECK(Coordinator::MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard(klassName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    generateCppPackageInclude(out, mPackage, "IHw" + baseName);
    out << "\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    out << "struct "
        << "Bn"
        << baseName
        << " : public ::android::hardware::BnInterface<I"
        << baseName << ", IHw" << baseName
        << ">, public ::android::hardware::HidlInstrumentor {\n";

    out.indent();
    out << "explicit Bn"
        << baseName
        << "(const ::android::sp<" << ifaceName << "> &_hidl_impl);"
        << "\n\n";
    out << "::android::status_t onTransact(\n";
    out.indent();
    out.indent();
    out << "uint32_t _hidl_code,\n";
    out << "const ::android::hardware::Parcel &_hidl_data,\n";
    out << "::android::hardware::Parcel *_hidl_reply,\n";
    out << "uint32_t _hidl_flags = 0,\n";
    out << "TransactCallback _hidl_cb = nullptr) override;\n\n";
    out.unindent();
    out.unindent();

    status_t err = generateMethods(out, [&](const Method *method, const Interface *) {
        return generateStubMethod(out, method);
    });

    if (err != OK) {
        return err;
    }


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

    const Interface *iface = mRootScope->getInterface();
    const std::string baseName = iface->getBaseName();

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));
    path.append("Bp");
    path.append(baseName);
    path.append(".h");

    CHECK(Coordinator::MakeParentHierarchy(path));
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

    generateCppPackageInclude(out, mPackage, "IHw" + baseName);
    out << "\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    out << "struct "
        << "Bp"
        << baseName
        << " : public ::android::hardware::BpInterface<IHw"
        << baseName
        << ">, public ::android::hardware::HidlInstrumentor {\n";

    out.indent();

    out << "explicit Bp"
        << baseName
        << "(const ::android::sp<::android::hardware::IBinder> &_hidl_impl);"
        << "\n\n";

    out << "virtual bool isRemote() const override { return true; }\n\n";

    status_t err = generateMethods(out, [&](const Method *method, const Interface *) {
        method->generateCppSignature(out);
        out << " override;\n";
        return OK;
    });

    if (err != OK) {
        return err;
    }

    out.unindent();
    out << "};\n\n";

    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::generateAllSource(const std::string &outputPath) const {

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));

    std::string ifaceName;
    std::string baseName;

    const Interface *iface = nullptr;
    bool isInterface;
    if (!AST::isInterface(&ifaceName)) {
        baseName = "types";
        isInterface = false;
    } else {
        iface = mRootScope->getInterface();
        baseName = iface->getBaseName();
        isInterface = true;
    }

    path.append(baseName);

    if (baseName != "types") {
        path.append("All");
    }

    path.append(".cpp");

    CHECK(Coordinator::MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    out << "#include <android/log.h>\n";
    out << "#include <cutils/trace.h>\n\n";
    if (isInterface) {
        // This is a no-op for IServiceManager itself.
        out << "#include <android/hidl/manager/1.0/IServiceManager.h>\n";

        generateCppPackageInclude(out, mPackage, "Bp" + baseName);
        generateCppPackageInclude(out, mPackage, "Bn" + baseName);
        generateCppPackageInclude(out, mPackage, "Bs" + baseName);

        for (const Interface *superType : iface->superTypeChain()) {
            generateCppPackageInclude(out,
                                      superType->fqName(),
                                      "Bp" + superType->getBaseName());
        }
    } else {
        generateCppPackageInclude(out, mPackage, "types");
        generateCppPackageInclude(out, mPackage, "hwtypes");
    }

    out << "\n";

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    status_t err = generateTypeSource(out, ifaceName);

    if (err == OK && isInterface) {
        const Interface *iface = mRootScope->getInterface();
        out << "constexpr ::android::hardware::hidl_version "
            << ifaceName
            << "::version;\n\n";

        // need to be put here, generateStubSource is using this.
        out << "const char* I"
            << iface->getBaseName()
            << "::descriptor(\""
            << iface->fqName().string()
            << "\");\n\n";

        out << "int I"
            << iface->getBaseName()
            << "::hidlStaticBlock = []() -> int {\n";
        out.indentBlock([&] {
            out << "::android::hardware::gBnConstructorMap[I"
                << iface->getBaseName()
                << "::descriptor]\n";
            out.indentBlock(2, [&] {
                out << "= [](void *iIntf) -> ::android::sp<::android::hardware::IBinder> {\n";
                out.indentBlock([&] {
                    out << "return new Bn"
                        << iface->getBaseName()
                        << "(reinterpret_cast<I"
                        << iface->getBaseName()
                        << " *>(iIntf));\n";
                });
                out << "};\n";
            });
            out << "return 1;\n";
        });
        out << "}();\n\n";

        err = generateInterfaceSource(out);
    }

    if (err == OK && isInterface) {
        err = generateProxySource(out, baseName);
    }

    if (err == OK && isInterface) {
        err = generateStubSource(out, baseName);
    }

    if (err == OK && isInterface) {
        err = generatePassthroughSource(out);
    }

    if (err == OK && isInterface) {
        const Interface *iface = mRootScope->getInterface();

        if (isIBase()) {
            out << "// skipped IMPLEMENT_SERVICE_MANAGER_INTERACTIONS\n";
        } else {
            out << "IMPLEMENT_SERVICE_MANAGER_INTERACTIONS("
                << baseName << ", "
                << "\"" << iface->fqName().package()
                << iface->fqName().atVersion()
                << "\")\n";
        }
    }

    enterLeaveNamespace(out, false /* enter */);

    return err;
}

// static
void AST::generateCheckNonNull(Formatter &out, const std::string &nonNull) {
    out << "if (" << nonNull << " == nullptr) {\n";
    out.indent();
    out << "return ::android::hardware::Status::fromExceptionCode(\n";
    out.indent();
    out.indent();
    out << "::android::hardware::Status::EX_ILLEGAL_ARGUMENT);\n";
    out.unindent();
    out.unindent();
    out.unindent();
    out << "}\n\n";
}

status_t AST::generateTypeSource(
        Formatter &out, const std::string &ifaceName) const {
    return mRootScope->emitTypeDefinitions(out, ifaceName);
}

void AST::declareCppReaderLocals(
        Formatter &out,
        const std::vector<TypedVar *> &args,
        bool forResults) const {
    if (args.empty()) {
        return;
    }

    for (const auto &arg : args) {
        const Type &type = arg->type();

        out << type.getCppResultType()
            << " "
            << (forResults ? "_hidl_out_" : "") + arg->name()
            << ";\n";
    }

    out << "\n";
}

void AST::emitCppReaderWriter(
        Formatter &out,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        const TypedVar *arg,
        bool isReader,
        Type::ErrorMode mode,
        bool addPrefixToName) const {
    const Type &type = arg->type();

    type.emitReaderWriter(
            out,
            addPrefixToName ? ("_hidl_out_" + arg->name()) : arg->name(),
            parcelObj,
            parcelObjIsPointer,
            isReader,
            mode);
}

void AST::emitCppResolveReferences(
        Formatter &out,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        const TypedVar *arg,
        bool isReader,
        Type::ErrorMode mode,
        bool addPrefixToName) const {
    const Type &type = arg->type();
    if(type.needsResolveReferences()) {
        type.emitResolveReferences(
                out,
                addPrefixToName ? ("_hidl_out_" + arg->name()) : arg->name(),
                isReader, // nameIsPointer
                parcelObj,
                parcelObjIsPointer,
                isReader,
                mode);
    }
}

status_t AST::generateProxyMethodSource(Formatter &out,
                                        const std::string &klassName,
                                        const Method *method,
                                        const Interface *superInterface) const {

    method->generateCppSignature(out,
                                 klassName,
                                 true /* specify namespaces */);

    const bool returnsValue = !method->results().empty();
    const TypedVar *elidedReturn = method->canElideCallback();

    out << " {\n";

    out.indent();

    if (returnsValue && elidedReturn == nullptr) {
        generateCheckNonNull(out, "_hidl_cb");
    }

    status_t status = generateCppInstrumentationCall(
            out,
            InstrumentationEvent::CLIENT_API_ENTRY,
            method);
    if (status != OK) {
        return status;
    }

    out << "::android::hardware::Parcel _hidl_data;\n";
    out << "::android::hardware::Parcel _hidl_reply;\n";
    out << "::android::status_t _hidl_err;\n";
    out << "::android::hardware::Status _hidl_status;\n\n";

    declareCppReaderLocals(
            out, method->results(), true /* forResults */);

    out << "_hidl_err = _hidl_data.writeInterfaceToken(";
    out << superInterface->fqName().cppNamespace()
        << "::I"
        << superInterface->getBaseName();
    out << "::descriptor);\n";
    out << "if (_hidl_err != ::android::OK) { goto _hidl_error; }\n\n";

    // First DFS: write all buffers and resolve pointers for parent
    for (const auto &arg : method->args()) {
        emitCppReaderWriter(
                out,
                "_hidl_data",
                false /* parcelObjIsPointer */,
                arg,
                false /* reader */,
                Type::ErrorMode_Goto,
                false /* addPrefixToName */);
    }

    // Second DFS: resolve references.
    for (const auto &arg : method->args()) {
        emitCppResolveReferences(
                out,
                "_hidl_data",
                false /* parcelObjIsPointer */,
                arg,
                false /* reader */,
                Type::ErrorMode_Goto,
                false /* addPrefixToName */);
    }

    out << "_hidl_err = remote()->transact("
        << method->getSerialId()
        << " /* "
        << method->name()
        << " */, _hidl_data, &_hidl_reply";

    if (method->isOneway()) {
        out << ", ::android::hardware::IBinder::FLAG_ONEWAY";
    }
    out << ");\n";

    out << "if (_hidl_err != ::android::OK) { goto _hidl_error; }\n\n";

    if (!method->isOneway()) {
        out << "_hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);\n";
        out << "if (_hidl_err != ::android::OK) { goto _hidl_error; }\n\n";
        out << "if (!_hidl_status.isOk()) { return _hidl_status; }\n\n";


        // First DFS: write all buffers and resolve pointers for parent
        for (const auto &arg : method->results()) {
            emitCppReaderWriter(
                    out,
                    "_hidl_reply",
                    false /* parcelObjIsPointer */,
                    arg,
                    true /* reader */,
                    Type::ErrorMode_Goto,
                    true /* addPrefixToName */);
        }

        // Second DFS: resolve references.
        for (const auto &arg : method->results()) {
            emitCppResolveReferences(
                    out,
                    "_hidl_reply",
                    false /* parcelObjIsPointer */,
                    arg,
                    true /* reader */,
                    Type::ErrorMode_Goto,
                    true /* addPrefixToName */);
        }

        if (returnsValue && elidedReturn == nullptr) {
            out << "_hidl_cb(";

            bool first = true;
            for (const auto &arg : method->results()) {
                if (!first) {
                    out << ", ";
                }

                if (arg->type().resultNeedsDeref()) {
                    out << "*";
                }
                out << "_hidl_out_" << arg->name();

                first = false;
            }

            out << ");\n\n";
        }
    }
    status = generateCppInstrumentationCall(
            out,
            InstrumentationEvent::CLIENT_API_EXIT,
            method);
    if (status != OK) {
        return status;
    }

    if (elidedReturn != nullptr) {
        out << "_hidl_status.setFromStatusT(_hidl_err);\n";
        out << "return ::android::hardware::Return<";
        out << elidedReturn->type().getCppResultType()
            << ">(_hidl_out_" << elidedReturn->name() << ");\n\n";
    } else {
        out << "_hidl_status.setFromStatusT(_hidl_err);\n";
        out << "return ::android::hardware::Return<void>();\n\n";
    }

    out.unindent();
    out << "_hidl_error:\n";
    out.indent();
    out << "_hidl_status.setFromStatusT(_hidl_err);\n";
    out << "return ::android::hardware::Return<";
    if (elidedReturn != nullptr) {
        out << method->results().at(0)->type().getCppResultType();
    } else {
        out << "void";
    }
    out << ">(_hidl_status);\n";

    out.unindent();
    out << "}\n\n";
    return OK;
}

status_t AST::generateProxySource(
        Formatter &out, const std::string &baseName) const {
    const std::string klassName = "Bp" + baseName;

    out << klassName
        << "::"
        << klassName
        << "(const ::android::sp<::android::hardware::IBinder> &_hidl_impl)\n";

    out.indent();
    out.indent();

    out << ": BpInterface"
        << "<IHw"
        << baseName
        << ">(_hidl_impl),\n"
        << "  ::android::hardware::HidlInstrumentor(\""
        << mPackage.string()
        << "::I"
        << baseName
        << "\") {\n";

    out.unindent();
    out.unindent();
    out << "}\n\n";

    status_t err = generateMethods(out, [&](const Method *method, const Interface *superInterface) {
        return generateProxyMethodSource(out, klassName, method, superInterface);
    });

    return err;
}

status_t AST::generateStubSource(
        Formatter &out, const std::string &baseName) const {
    out << "IMPLEMENT_HWBINDER_META_INTERFACE("
        << baseName
        << ", "
        << mPackage.cppNamespace()
        << "::I"
        << baseName
        << "::descriptor);\n\n";

    const std::string klassName = "Bn" + baseName;

    out << klassName
        << "::"
        << klassName
        << "(const ::android::sp<I" << baseName <<"> &_hidl_impl)\n";

    out.indent();
    out.indent();

    out << ": BnInterface"
        << "<I"
        << baseName
        << ", IHw"
        << baseName
        << ">(_hidl_impl),\n"
        << "  ::android::hardware::HidlInstrumentor(\""
        << mPackage.string()
        << "::I"
        << baseName
        << "\") {\n";

    out.unindent();
    out.unindent();
    out << "}\n\n";

    out << "::android::status_t " << klassName << "::onTransact(\n";

    out.indent();
    out.indent();

    out << "uint32_t _hidl_code,\n"
        << "const ::android::hardware::Parcel &_hidl_data,\n"
        << "::android::hardware::Parcel *_hidl_reply,\n"
        << "uint32_t _hidl_flags,\n"
        << "TransactCallback _hidl_cb) {\n";

    out.unindent();

    out << "::android::status_t _hidl_err = ::android::OK;\n\n";
    out << "switch (_hidl_code) {\n";
    out.indent();

    const Interface *iface = mRootScope->getInterface();

    for (const auto &tuple : iface->allMethodsFromRoot()) {
        const Method *method = tuple.method();
        const Interface *superInterface = tuple.interface();
        out << "case "
            << method->getSerialId()
            << " /* "
            << method->name()
            << " */:\n{\n";

        out.indent();

        status_t err =
            generateStubSourceForMethod(out, superInterface, method);

        if (err != OK) {
            return err;
        }

        out.unindent();
        out << "}\n\n";
    }

    out << "default:\n{\n";
    out.indent();

    out << "return ::android::hardware::BnInterface<I"
        << baseName << ", IHw" << baseName
        << ">::onTransact(\n";

    out.indent();
    out.indent();

    out << "_hidl_code, _hidl_data, _hidl_reply, "
        << "_hidl_flags, _hidl_cb);\n";

    out.unindent();
    out.unindent();

    out.unindent();
    out << "}\n";

    out.unindent();
    out << "}\n\n";

    out << "if (_hidl_err == ::android::UNEXPECTED_NULL) {\n";
    out.indent();
    out << "_hidl_err = ::android::hardware::writeToParcel(\n";
    out.indent();
    out.indent();
    out << "::android::hardware::Status::fromExceptionCode(::android::hardware::Status::EX_NULL_POINTER),\n";
    out << "_hidl_reply);\n";
    out.unindent();
    out.unindent();

    out.unindent();
    out << "}\n\n";

    out << "return _hidl_err;\n";

    out.unindent();
    out << "}\n\n";

    return OK;
}

status_t AST::generateStubSourceForMethod(
        Formatter &out, const Interface *iface, const Method *method) const {
    out << "if (!_hidl_data.enforceInterface(";

    out << iface->fqName().cppNamespace()
        << "::I"
        << iface->getBaseName();

    out << "::descriptor)) {\n";

    out.indent();
    out << "_hidl_err = ::android::BAD_TYPE;\n";
    out << "break;\n";
    out.unindent();
    out << "}\n\n";

    declareCppReaderLocals(out, method->args(), false /* forResults */);

    // First DFS: write buffers
    for (const auto &arg : method->args()) {
        emitCppReaderWriter(
                out,
                "_hidl_data",
                false /* parcelObjIsPointer */,
                arg,
                true /* reader */,
                Type::ErrorMode_Break,
                false /* addPrefixToName */);
    }

    // Second DFS: resolve references
    for (const auto &arg : method->args()) {
        emitCppResolveReferences(
                out,
                "_hidl_data",
                false /* parcelObjIsPointer */,
                arg,
                true /* reader */,
                Type::ErrorMode_Break,
                false /* addPrefixToName */);
    }

    status_t status = generateCppInstrumentationCall(
            out,
            InstrumentationEvent::SERVER_API_ENTRY,
            method);
    if (status != OK) {
        return status;
    }

    const bool returnsValue = !method->results().empty();
    const TypedVar *elidedReturn = method->canElideCallback();

    if (elidedReturn != nullptr) {
        out << elidedReturn->type().getCppResultType()
            << " "
            << elidedReturn->name()
            << " = "
            << method->name()
            << "(";

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

        out << ");\n\n";
        out << "::android::hardware::writeToParcel(::android::hardware::Status::ok(), "
            << "_hidl_reply);\n\n";

        elidedReturn->type().emitReaderWriter(
                out,
                elidedReturn->name(),
                "_hidl_reply",
                true, /* parcelObjIsPointer */
                false, /* isReader */
                Type::ErrorMode_Ignore);

        emitCppResolveReferences(
                out,
                "_hidl_reply",
                true /* parcelObjIsPointer */,
                elidedReturn,
                false /* reader */,
                Type::ErrorMode_Ignore,
                false /* addPrefixToName */);

        status_t status = generateCppInstrumentationCall(
                out,
                InstrumentationEvent::SERVER_API_EXIT,
                method);
        if (status != OK) {
            return status;
        }

        out << "_hidl_cb(*_hidl_reply);\n";
    } else {
        if (returnsValue) {
            out << "bool _hidl_callbackCalled = false;\n\n";
        }

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
            out << "if (_hidl_callbackCalled) {\n";
            out.indent();
            out << "LOG_ALWAYS_FATAL(\""
                << method->name()
                << ": _hidl_cb called a second time, but must be called once.\");\n";
            out.unindent();
            out << "}\n";
            out << "_hidl_callbackCalled = true;\n\n";

            out << "::android::hardware::writeToParcel(::android::hardware::Status::ok(), "
                << "_hidl_reply);\n\n";

            // First DFS: buffers
            for (const auto &arg : method->results()) {
                emitCppReaderWriter(
                        out,
                        "_hidl_reply",
                        true /* parcelObjIsPointer */,
                        arg,
                        false /* reader */,
                        Type::ErrorMode_Ignore,
                        false /* addPrefixToName */);
            }

            // Second DFS: resolve references
            for (const auto &arg : method->results()) {
                emitCppResolveReferences(
                        out,
                        "_hidl_reply",
                        true /* parcelObjIsPointer */,
                        arg,
                        false /* reader */,
                        Type::ErrorMode_Ignore,
                        false /* addPrefixToName */);
            }

            status_t status = generateCppInstrumentationCall(
                    out,
                    InstrumentationEvent::SERVER_API_EXIT,
                    method);
            if (status != OK) {
                return status;
            }

            out << "_hidl_cb(*_hidl_reply);\n";

            out.unindent();
            out << "}\n";
        }
        out << ");\n\n";

        if (returnsValue) {
            out << "if (!_hidl_callbackCalled) {\n";
            out.indent();
            out << "LOG_ALWAYS_FATAL(\""
                << method->name()
                << ": _hidl_cb not called, but must be called once.\");\n";
            out.unindent();
            out << "}\n\n";
        } else {
            out << "::android::hardware::writeToParcel("
                << "::android::hardware::Status::ok(), "
                << "_hidl_reply);\n\n";
        }
    }

    out << "break;\n";

    return OK;
}

status_t AST::generatePassthroughHeader(const std::string &outputPath) const {
    std::string ifaceName;
    if (!AST::isInterface(&ifaceName)) {
        // types.hal does not get a stub header.
        return OK;
    }

    const Interface *iface = mRootScope->getInterface();

    const std::string baseName = iface->getBaseName();
    const std::string klassName = "Bs" + baseName;

    bool supportOneway = iface->hasOnewayMethods();

    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));
    path.append(klassName);
    path.append(".h");

    CHECK(Coordinator::MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    const std::string guard = makeHeaderGuard(klassName);

    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    std::vector<std::string> packageComponents;
    getPackageAndVersionComponents(
            &packageComponents, false /* cpp_compatible */);

    out << "#include <future>\n";

    generateCppPackageInclude(out, mPackage, ifaceName);
    out << "\n";

    if (supportOneway) {
        out << "#include <hidl/TaskRunner.h>\n";
    }

    enterLeaveNamespace(out, true /* enter */);
    out << "\n";

    out << "struct "
        << klassName
        << " : " << ifaceName
        << ", ::android::hardware::HidlInstrumentor {\n";

    out.indent();
    out << "explicit "
        << klassName
        << "(const ::android::sp<"
        << ifaceName
        << "> impl);\n";

    status_t err = generateMethods(out, [&](const Method *method, const Interface *) {
        return generatePassthroughMethod(out, method);
    });

    if (err != OK) {
        return err;
    }

    out.unindent();
    out << "private:\n";
    out.indent();
    out << "const ::android::sp<" << ifaceName << "> mImpl;\n";

    if (supportOneway) {
        out << "::android::hardware::TaskRunner mOnewayQueue;\n";

        out << "\n";

        out << "::android::hardware::Return<void> addOnewayTask("
               "std::function<void(void)>);\n\n";
    }

    out.unindent();

    out << "};\n\n";

    enterLeaveNamespace(out, false /* enter */);

    out << "\n#endif  // " << guard << "\n";

    return OK;
}

status_t AST::generateInterfaceSource(Formatter &out) const {
    const Interface *iface = mRootScope->getInterface();

    // generate castFrom functions
    if (!iface->isRootType()) {
        std::string childTypeResult = iface->getCppResultType();

        for (const Interface *superType : iface->superTypeChain()) {
            out << "// static \n"
                << childTypeResult
                << " I"
                << iface->getBaseName()
                << "::castFrom("
                << superType->getCppArgumentType()
                << " parent) {\n";
            out.indent();
            out << "return ::android::hardware::castInterface<";
            out << "I" << iface->getBaseName() << ", "
                << superType->fqName().cppName() << ", "
                << "Bp" << iface->getBaseName() << ", "
                << superType->getHwName().cppName()
                << ">(\n";
            out.indent();
            out.indent();
            out << "parent, \""
                << iface->fqName().string()
                << "\");\n";
            out.unindent();
            out.unindent();
            out.unindent();
            out << "}\n\n";
        }
    }

    return OK;
}

status_t AST::generatePassthroughSource(Formatter &out) const {
    const Interface *iface = mRootScope->getInterface();

    const std::string baseName = iface->getBaseName();
    const std::string klassName = "Bs" + baseName;

    out << klassName
        << "::"
        << klassName
        << "(const ::android::sp<"
        << iface->fullName()
        << "> impl) : ::android::hardware::HidlInstrumentor(\""
        << iface->fqName().string()
        << "\"), mImpl(impl) {";
    if (iface->hasOnewayMethods()) {
        out << "\n";
        out.indentBlock([&] {
            out << "mOnewayQueue.setLimit(3000 /* similar limit to binderized */);\n";
        });
    }
    out << "}\n\n";

    if (iface->hasOnewayMethods()) {
        out << "::android::hardware::Return<void> "
            << klassName
            << "::addOnewayTask(std::function<void(void)> fun) {\n";
        out.indent();
        out << "if (!mOnewayQueue.push(fun)) {\n";
        out.indent();
        out << "return ::android::hardware::Status::fromExceptionCode(\n";
        out.indent();
        out.indent();
        out << "::android::hardware::Status::EX_TRANSACTION_FAILED);\n";
        out.unindent();
        out.unindent();
        out.unindent();
        out << "}\n";

        out << "return ::android::hardware::Status();\n";

        out.unindent();
        out << "}\n\n";


    }

    return OK;
}

status_t AST::generateCppAtraceCall(Formatter &out,
                                    InstrumentationEvent event,
                                    const Method *method) const {
    const Interface *iface = mRootScope->getInterface();
    std::string baseString = "HIDL::I" + iface->getBaseName() + "::" + method->name();
    switch (event) {
        case SERVER_API_ENTRY:
        {
            out << "atrace_begin(ATRACE_TAG_HAL, \""
                << baseString + "::server\");\n";
            break;
        }
        case CLIENT_API_ENTRY:
        {
            out << "atrace_begin(ATRACE_TAG_HAL, \""
                << baseString + "::client\");\n";
            break;
        }
        case PASSTHROUGH_ENTRY:
        {
            out << "atrace_begin(ATRACE_TAG_HAL, \""
                << baseString + "::passthrough\");\n";
            break;
        }
        case SERVER_API_EXIT:
        case CLIENT_API_EXIT:
        case PASSTHROUGH_EXIT:
        {
            out << "atrace_end(ATRACE_TAG_HAL);\n";
            break;
        }
        default:
        {
            LOG(ERROR) << "Unsupported instrumentation event: " << event;
            return UNKNOWN_ERROR;
        }
    }

    return OK;
}

status_t AST::generateCppInstrumentationCall(
        Formatter &out,
        InstrumentationEvent event,
        const Method *method) const {
    status_t err = generateCppAtraceCall(out, event, method);
    if (err != OK) {
        return err;
    }

    out << "if (UNLIKELY(mEnableInstrumentation)) {\n";
    out.indent();
    out << "std::vector<void *> _hidl_args;\n";
    std::string event_str = "";
    switch (event) {
        case SERVER_API_ENTRY:
        {
            event_str = "InstrumentationEvent::SERVER_API_ENTRY";
            for (const auto &arg : method->args()) {
                out << "_hidl_args.push_back((void *)"
                    << (arg->type().resultNeedsDeref() ? "" : "&")
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        case SERVER_API_EXIT:
        {
            event_str = "InstrumentationEvent::SERVER_API_EXIT";
            for (const auto &arg : method->results()) {
                out << "_hidl_args.push_back((void *)&"
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        case CLIENT_API_ENTRY:
        {
            event_str = "InstrumentationEvent::CLIENT_API_ENTRY";
            for (const auto &arg : method->args()) {
                out << "_hidl_args.push_back((void *)&"
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        case CLIENT_API_EXIT:
        {
            event_str = "InstrumentationEvent::CLIENT_API_EXIT";
            for (const auto &arg : method->results()) {
                out << "_hidl_args.push_back((void *)"
                    << (arg->type().resultNeedsDeref() ? "" : "&")
                    << "_hidl_out_"
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        case PASSTHROUGH_ENTRY:
        {
            event_str = "InstrumentationEvent::PASSTHROUGH_ENTRY";
            for (const auto &arg : method->args()) {
                out << "_hidl_args.push_back((void *)&"
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        case PASSTHROUGH_EXIT:
        {
            event_str = "InstrumentationEvent::PASSTHROUGH_EXIT";
            for (const auto &arg : method->results()) {
                out << "_hidl_args.push_back((void *)&"
                    << arg->name()
                    << ");\n";
            }
            break;
        }
        default:
        {
            LOG(ERROR) << "Unsupported instrumentation event: " << event;
            return UNKNOWN_ERROR;
        }
    }

    const Interface *iface = mRootScope->getInterface();

    out << "for (const auto &callback: mInstrumentationCallbacks) {\n";
    out.indent();
    out << "callback("
        << event_str
        << ", \""
        << mPackage.package()
        << "\", \""
        << mPackage.version()
        << "\", \""
        << iface->localName()
        << "\", \""
        << method->name()
        << "\", &_hidl_args);\n";
    out.unindent();
    out << "}\n";
    out.unindent();
    out << "}\n\n";

    return OK;
}

}  // namespace android

