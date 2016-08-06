#include "AST.h"

#include "Coordinator.h"
#include "Formatter.h"
#include "FQName.h"
#include "HandleType.h"
#include "Scope.h"
#include "TypeDef.h"

#include <android-base/logging.h>
#include <stdlib.h>

namespace android {

AST::AST(Coordinator *coordinator)
    : mCoordinator(coordinator),
      mScanner(NULL),
      mRootScope(new Scope) {
    enterScope(mRootScope);
}

AST::~AST() {
    delete mRootScope;
    mRootScope = NULL;

    CHECK(mScanner == NULL);

    // Ownership of "coordinator" was NOT transferred.
}

void *AST::scanner() {
    return mScanner;
}

void AST::setScanner(void *scanner) {
    mScanner = scanner;
}

bool AST::setPackage(const char *package) {
    mPackage.setTo(package);
    CHECK(mPackage.isValid());

    if (mPackage.package().empty()
            || mPackage.version().empty()
            || !mPackage.name().empty()) {
        return false;
    }

    return true;
}

FQName AST::package() const {
    return mPackage;
}

bool AST::isInterface(std::string *ifaceName) const {
    return mRootScope->containsSingleInterface(ifaceName);
}

bool AST::addImport(const char *import) {
    FQName fqName(import);
    CHECK(fqName.isValid());

    fqName.applyDefaults(mPackage.package(), mPackage.version());

    // LOG(INFO) << "importing " << fqName.string();

    if (fqName.name().empty()) {
        std::vector<FQName> packageInterfaces;

        status_t err =
            mCoordinator->getPackageInterfaces(fqName, &packageInterfaces);

        if (err != OK) {
            return false;
        }

        for (const auto &subFQName : packageInterfaces) {
            if (mCoordinator->parse(subFQName) == NULL) {
                return false;
            }
        }

        return true;
    }

    AST *importAST = mCoordinator->parse(fqName);

    if (importAST == NULL) {
        return false;
    }

    return true;
}

void AST::enterScope(Scope *container) {
    mScopePath.push_back(container);
}

void AST::leaveScope() {
    mScopePath.pop();
}

Scope *AST::scope() {
    CHECK(!mScopePath.empty());
    return mScopePath.top();
}

bool AST::addScopedType(const char *localName, NamedType *type) {
    // LOG(INFO) << "adding scoped type '" << localName << "'";

    bool success = scope()->addType(localName, type);
    if (!success) {
        return false;
    }

    std::string path;
    for (size_t i = 1; i < mScopePath.size(); ++i) {
        path.append(mScopePath[i]->localName());
        path.append(".");
    }
    path.append(localName);

    type->setLocalName(localName);

    FQName fqName(mPackage.package(), mPackage.version(), path);
    type->setFullName(fqName);

    return true;
}

Type *AST::lookupType(const char *name) {
    FQName fqName(name);
    CHECK(fqName.isValid());

    if (fqName.name().empty()) {
        // Given a package and version???
        return NULL;
    }

    if (fqName.package().empty() && fqName.version().empty()) {
        // This is just a plain identifier, resolve locally first if possible.

        for (size_t i = mScopePath.size(); i-- > 0;) {
            Type *type = mScopePath[i]->lookupType(name);

            if (type != NULL) {
                // Resolve typeDefs to the target type.
                while (type->isTypeDef()) {
                    type = static_cast<TypeDef *>(type)->referencedType();
                }

                return type->ref();
            }
        }
    }

    fqName.applyDefaults(mPackage.package(), mPackage.version());

    // LOG(INFO) << "lookupType now looking for " << fqName.string();

    Type *resultType = mCoordinator->lookupType(fqName);

    if (resultType) {
        if (!resultType->isInterface()) {
            // Non-interface types are declared in the associated types header.
            FQName typesName(fqName.package(), fqName.version(), "types");
            mImportedNames.insert(typesName);
        } else {
            mImportedNames.insert(fqName);
        }
    }

    return resultType;
}

Type *AST::lookupTypeInternal(const std::string &namePath) const {
    Scope *scope = mRootScope;

    size_t startPos = 0;
    for (;;) {
        size_t dotPos = namePath.find('.', startPos);

        std::string component;
        if (dotPos == std::string::npos) {
            component = namePath.substr(startPos);
        } else {
            component = namePath.substr(startPos, dotPos - startPos);
        }

        Type *type = scope->lookupType(component.c_str());

        if (type == NULL) {
            return NULL;
        }

        if (dotPos == std::string::npos) {
            // Resolve typeDefs to the target type.
            while (type->isTypeDef()) {
                type = static_cast<TypeDef *>(type)->referencedType();
            }

            return type;
        }

        if (!type->isScope()) {
            return NULL;
        }

        scope = static_cast<Scope *>(type);
        startPos = dotPos + 1;
    }
}

void AST::addImportedPackages(std::set<FQName> *importSet) const {
    for (const auto &fqName : mImportedNames) {
        FQName packageName(fqName.package(), fqName.version(), "");

        if (packageName == mPackage) {
            // We only care about external imports, not our own package.
            continue;
        }

        importSet->insert(packageName);
    }
}

}  // namespace android;
