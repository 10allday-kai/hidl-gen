#include "AST.h"

#include "Annotation.h"
#include "Coordinator.h"
#include "Formatter.h"
#include "Interface.h"
#include "Method.h"
#include "Scope.h"

#include <android-base/logging.h>
#include <string>
#include <vector>

namespace android {

// Remove the double quotas in a string.
static std::string removeQuotes(const std::string in) {
    std::string out{in};
    return out.substr(1, out.size() - 2);
}

status_t AST::emitVtsTypeDeclarations(
        Formatter &out,
        Vector<Type*> types) const {
    for (const auto& type : types) {
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

status_t AST::generateVts(const std::string &outputPath) const {
    std::string path = outputPath;
    path.append(mCoordinator->convertPackageRootToPath(mPackage));
    path.append(mCoordinator->getPackagePath(mPackage, true /* relative */));

    std::string ifaceName;
    std::string baseName;

    bool isInterface = true;
    if (!AST::isInterface(&ifaceName)) {
        baseName = "types";
        isInterface = false;
    } else {
        baseName = ifaceName.substr(1);  // cut off the leading 'I'.
    }

    path.append(baseName);
    path.append(".vts");

    CHECK(Coordinator::MakeParentHierarchy(path));
    FILE *file = fopen(path.c_str(), "w");

    if (file == NULL) {
        return -errno;
    }

    Formatter out(file);

    out << "component_class: HAL_HIDL\n";

    // Get the component_type for interface from annotation.
    if (isInterface) {
        const Interface *iface = mRootScope->getInterface();
        Annotation *annotation = iface->annotations().valueFor("hal_type");
        if (annotation != NULL) {
            std::vector<std::string> * values = annotation->params().valueFor(
                    "type");
            if (values != NULL) {
                out << "component_type: "
                    << removeQuotes(values->at(0))
                    << "\n";
            }
        }
    }

    out << "component_type_version: " << mPackage.version().substr(1) << "\n";
    out << "component_name: \""
        << (isInterface ? ifaceName : "types")
        << "\"\n\n";

    out << "package: \"" << mPackage.package() << "\"\n\n";

    for (const auto &item : mImportedNames) {
        out << "import: \"" << item.string() << "\"\n";
    }

    out << "\n";

    if (isInterface) {
        const Interface *iface = mRootScope->getInterface();
        out << "interface: {\n";
        out.indent();

        status_t status = emitVtsTypeDeclarations(out, iface->getSubTypes());
        if (status != OK) {
            return status;
        }

        for (const auto &method : iface->methods()) {
            out << "api: {\n";
            out.indent();
            out << "name: \"" << method->name() << "\"\n";
            for (const auto &result : method->results()) {
                out << "return_type_hidl: {\n";
                out.indent();
                status_t status = result->type().emitVtsArgumentType(out);
                if (status != OK) {
                    return status;
                }
                out.unindent();
                out << "}\n";
            }
            for (const auto &arg : method->args()) {
                out << "arg: {\n";
                out.indent();
                status_t status = arg->type().emitVtsArgumentType(out);
                if (status != OK) {
                    return status;
                }
                out.unindent();
                out << "}\n";
            }
            out.unindent();
            out << "}\n\n";
        }

        out.unindent();
        out << "}\n";
    } else {
        status_t status = emitVtsTypeDeclarations(out,
                                                  mRootScope->getSubTypes());
        if (status != OK) {
            return status;
        }
    }
    return OK;
}

}  // namespace android




