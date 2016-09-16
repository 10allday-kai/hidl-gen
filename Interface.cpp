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

#include "Interface.h"

#include "Annotation.h"
#include "Method.h"

#include <hidl-util/Formatter.h>
#include <iostream>

namespace android {

Interface::Interface(
        const char *localName, Interface *super,
        std::vector<Annotation *> *annotations)
    : Scope(localName),
      mSuperType(super),
      mAnnotations(annotations),
      mIsJavaCompatibleInProgress(false) {
}

void Interface::addMethod(Method *method) {
    mMethods.push_back(method);
}

const Interface *Interface::superType() const {
    return mSuperType;
}

bool Interface::isInterface() const {
    return true;
}

bool Interface::isBinder() const {
    return true;
}

const std::vector<Method *> &Interface::methods() const {
    return mMethods;
}

const std::vector<Annotation *> &Interface::annotations() const {
    return *mAnnotations;
}

std::string Interface::getBaseName() const {
    // cut off the leading 'I'.
    return localName().substr(1);
}

std::string Interface::getCppType(StorageMode mode,
                                  std::string *extra,
                                  bool specifyNamespaces) const {
    extra->clear();
    const std::string base =
          std::string(specifyNamespaces ? "::android::" : "")
        + "sp<"
        + (specifyNamespaces ? fullName() : partialCppName())
        + ">";

    switch (mode) {
        case StorageMode_Stack:
        case StorageMode_Result:
            return base;

        case StorageMode_Argument:
            return "const " + base + "&";
    }
}

std::string Interface::getJavaType(
        std::string *extra, bool /* forInitializer */) const {
    extra->clear();
    return fullJavaName();
}

void Interface::emitReaderWriter(
        Formatter &out,
        const std::string &name,
        const std::string &parcelObj,
        bool parcelObjIsPointer,
        bool isReader,
        ErrorMode mode) const {
    const std::string parcelObjDeref =
        parcelObj + (parcelObjIsPointer ? "->" : ".");

    if (isReader) {
        out << "{\n";
        out.indent();

        const std::string binderName = "_hidl_" + name + "_binder";

        out << "::android::sp<::android::hardware::IBinder> "
            << binderName << ";\n";

        out << "_hidl_err = ";
        out << parcelObjDeref
            << "readNullableStrongBinder(&"
            << binderName
            << ");\n";

        handleError(out, mode);

        out << name
            << " = "
            << fqName().cppNamespace()
            << "::IHw"
            << getBaseName()
            << "::asInterface("
            << binderName
            << ");\n";

        out.unindent();
        out << "}\n\n";
    } else {

        out << "if (" << name << "->isRemote()) {\n";
        out.indent();
        out << "_hidl_err = ";
        out << parcelObjDeref
            << "writeStrongBinder("
            << fqName().cppNamespace()
            << "::IHw"
            << getBaseName()
            << "::asBinder(static_cast<"
            << fqName().cppNamespace()
            << "::IHw"
            << getBaseName()
            << "*>("
            << name << ".get()"
            << ")));\n";
        out.unindent();
        out << "} else {\n";
        out.indent();
        out << "_hidl_err = ";
        out << parcelObjDeref
            << "writeStrongBinder("
            << "new " << fqName().cppNamespace()
            << "::Bn" << getBaseName() << " "
            << "(" << name <<"));\n";
        out.unindent();
        out << "}\n";
        handleError(out, mode);
    }
}

void Interface::emitJavaReaderWriter(
        Formatter &out,
        const std::string &parcelObj,
        const std::string &argName,
        bool isReader) const {
    if (isReader) {
        out << fullJavaName()
            << ".asInterface("
            << parcelObj
            << ".readStrongBinder());\n";
    } else {
        out << parcelObj
            << ".writeStrongBinder("
            << argName
            << " == null ? null : "
            << argName
            << ".asBinder());\n";
    }
}

status_t Interface::emitVtsAttributeDeclaration(Formatter &out) const {
    for (const auto &type : getSubTypes()) {
        out << "attribute: {\n";
        out.indent();
        status_t status = type->emitVtsTypeDeclarations(out);
        if (status != OK) {
            return status;
        }
        out.unindent();
        out << "}\n\n";
    }
    return OK;
}

status_t Interface::emitVtsMethodDeclaration(Formatter &out) const {
    for (const auto &method : mMethods) {
        out << "api: {\n";
        out.indent();
        out << "name: \"" << method->name() << "\"\n";
        // Generate declaration for each return value.
        for (const auto &result : method->results()) {
            out << "return_type_hidl: {\n";
            out.indent();
            status_t status = result->type().emitVtsAttributeType(out);
            if (status != OK) {
                return status;
            }
            out.unindent();
            out << "}\n";
        }
        // Generate declaration for each input argument
        for (const auto &arg : method->args()) {
            out << "arg: {\n";
            out.indent();
            status_t status = arg->type().emitVtsAttributeType(out);
            if (status != OK) {
                return status;
            }
            out.unindent();
            out << "}\n";
        }
        // Generate declaration for each annotation.
        for (const auto &annotation : method->annotations()) {
            out << "callflow: {\n";
            out.indent();
            std::string name = annotation->name();
            if (name == "entry") {
                out << "entry: true\n";
            } else if (name == "exit") {
                out << "exit: true\n";
            } else if (name == "callflow") {
                const AnnotationParam *param =
                        annotation->getParam("next");
                if (param != nullptr) {
                    for (auto value : *param->getValues()) {
                        out << "next: " << value << "\n";
                    }
                }
            } else {
                std::cerr << "Invalid annotation '"
                          << name << "' for method: " << method->name()
                          << ". Should be one of: entry, exit, callflow. \n";
                return UNKNOWN_ERROR;
            }
            out.unindent();
            out << "}\n";
        }
        out.unindent();
        out << "}\n\n";
    }
    return OK;
}

status_t Interface::emitVtsAttributeType(Formatter &out) const {
    out << "type: TYPE_HIDL_CALLBACK\n"
        << "predefined_type: \""
        << localName()
        << "\"\n"
        << "is_callback: true\n";
    return OK;
}

bool Interface::isJavaCompatible() const {
    if (mIsJavaCompatibleInProgress) {
        // We're currently trying to determine if this Interface is
        // java-compatible and something is referencing this interface through
        // one of its methods. Assume we'll ultimately succeed, if we were wrong
        // the original invocation of Interface::isJavaCompatible() will then
        // return the correct "false" result.
        return true;
    }

    if (mSuperType != nullptr && !mSuperType->isJavaCompatible()) {
        mIsJavaCompatibleInProgress = false;
        return false;
    }

    mIsJavaCompatibleInProgress = true;

    if (!Scope::isJavaCompatible()) {
        mIsJavaCompatibleInProgress = false;
        return false;
    }

    for (const auto &method : mMethods) {
        if (!method->isJavaCompatible()) {
            mIsJavaCompatibleInProgress = false;
            return false;
        }
    }

    mIsJavaCompatibleInProgress = false;

    return true;
}

}  // namespace android

