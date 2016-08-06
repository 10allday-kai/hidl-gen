#include "Coordinator.h"

#include "AST.h"
#include "RefType.h"

#include <android-base/logging.h>

extern android::status_t parseFile(android::AST *ast, const char *path);

namespace android {

Coordinator::Coordinator(const std::string &interfacesPath)
    : mInterfacesPath(interfacesPath) {
}

Coordinator::~Coordinator() {}

AST *Coordinator::parse(const FQName &fqName) {
    CHECK(fqName.isFullyQualified());

    // LOG(INFO) << "parsing " << fqName.string();

    ssize_t index = mCache.indexOfKey(fqName);
    if (index >= 0) {
        AST *ast = mCache.valueAt(index);

        return ast;
    }

    // Add this to the cache immediately, so we can discover circular imports.
    mCache.add(fqName, NULL);

    const std::string packagePath = getPackagePath(fqName);

    if (fqName.name() != "types") {
        // Any interface file implicitly imports its package's types.hal.
        FQName typesName(fqName.package(), fqName.version(), "types");
        (void)parse(typesName);

        // fall through.
    }

    std::string path = packagePath;
    path.append(fqName.name());
    path.append(".hal");

    AST *ast = new AST(this);
    status_t err = parseFile(ast, path.c_str());

    if (err != OK) {
        // LOG(ERROR) << "parsing '" << path << "' FAILED.";

        delete ast;
        ast = NULL;

        return NULL;
    }

    if (ast->package().package() != fqName.package()
            || ast->package().version() != fqName.version()) {
        LOG(ERROR)
            << "File at '" << path << "' does not match expected package "
            << "and/or version.";

        err = UNKNOWN_ERROR;
    } else {
        std::string ifaceName;
        if (ast->isInterface(&ifaceName)) {
            if (fqName.name() == "types") {
                LOG(ERROR)
                    << "File at '" << path << "' declares an interface '"
                    << ifaceName
                    << "' instead of the expected types common to the package.";

                err = UNKNOWN_ERROR;
            } else if (ifaceName != fqName.name()) {
                LOG(ERROR)
                    << "File at '" << path << "' does not declare interface type '"
                    << fqName.name()
                    << "'.";

                err = UNKNOWN_ERROR;
            }
        } else if (fqName.name() != "types") {
            LOG(ERROR)
                << "File at '" << path << "' declares types rather than the "
                << "expected interface type '" << fqName.name() << "'.";

            err = UNKNOWN_ERROR;
        }
    }

    if (err != OK) {
        delete ast;
        ast = NULL;

        return NULL;
    }

    mCache.add(fqName, ast);

    return ast;
}

std::string Coordinator::getPackagePath(
        const FQName &fqName, bool relative) const {
    CHECK(!fqName.package().empty());
    CHECK(!fqName.version().empty());
    const char *const kPrefix = "android.hardware.";
    CHECK_EQ(fqName.package().find(kPrefix), 0u);

    const std::string packageSuffix = fqName.package().substr(strlen(kPrefix));

    std::string packagePath;
    if (!relative) {
        packagePath = mInterfacesPath;
    }

    size_t startPos = 0;
    size_t dotPos;
    while ((dotPos = packageSuffix.find('.', startPos)) != std::string::npos) {
        packagePath.append(packageSuffix.substr(startPos, dotPos - startPos));
        packagePath.append("/");

        startPos = dotPos + 1;
    }
    CHECK_LT(startPos + 1, packageSuffix.length());
    packagePath.append(packageSuffix.substr(startPos));
    packagePath.append("/");

    CHECK_EQ(fqName.version().find('@'), 0u);
    packagePath.append(fqName.version().substr(1));
    packagePath.append("/");

    return packagePath;
}

Type *Coordinator::lookupType(const FQName &fqName) const {
    // Fully qualified.
    CHECK(fqName.isFullyQualified());

    std::string topType;
    size_t dotPos = fqName.name().find('.');
    if (dotPos == std::string::npos) {
        topType = fqName.name();
    } else {
        topType = fqName.name().substr(0, dotPos);
    }

    // Assuming {topType} is the name of an interface type, let's see if the
    // associated {topType}.hal file was imported.
    FQName ifaceName(fqName.package(), fqName.version(), topType);
    ssize_t index = mCache.indexOfKey(ifaceName);
    if (index >= 0) {
        AST *ast = mCache.valueAt(index);
        CHECK(ast != NULL);

        Type *type = ast->lookupTypeInternal(fqName.name());

        if (type != NULL) {
            return new RefType(fqName.string().c_str(), type);
        }
    }

    FQName typesName(fqName.package(), fqName.version(), "types");
    index = mCache.indexOfKey(typesName);
    if (index >= 0) {
        AST *ast = mCache.valueAt(index);
        if (ast != NULL) {
            // ast could be NULL if types.hal didn't exist, which is valid.
            Type *type = ast->lookupTypeInternal(fqName.name());

            if (type != NULL) {
                return new RefType(fqName.string().c_str(), type);
            }
        }
    }

    return NULL;
}

}  // namespace android

