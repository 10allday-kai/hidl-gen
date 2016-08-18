#include "Coordinator.h"

#include "AST.h"

#include <android-base/logging.h>
#include <iterator>
#include <sys/dir.h>
#include <sys/stat.h>

extern android::status_t parseFile(android::AST *ast);

namespace android {

Coordinator::Coordinator(
        const std::vector<std::string> &packageRootPaths,
        const std::vector<std::string> &packageRoots)
    : mPackageRootPaths(packageRootPaths),
      mPackageRoots(packageRoots) {
    // empty
}

Coordinator::~Coordinator() {
    // empty
}

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

    if (fqName.name() != "types") {
        // Any interface file implicitly imports its package's types.hal.
        FQName typesName(fqName.package(), fqName.version(), "types");
        (void)parse(typesName);

        // fall through.
    }

    std::string path = getPackagePath(fqName);

    path.append(fqName.name());
    path.append(".hal");

    AST *ast = new AST(this, path);
    status_t err = parseFile(ast);

    if (err != OK) {
        // LOG(ERROR) << "parsing '" << path << "' FAILED.";

        delete ast;
        ast = NULL;

        return NULL;
    }

    if (ast->package().package() != fqName.package()
            || ast->package().version() != fqName.version()) {
        fprintf(stderr,
                "ERROR: File at '%s' does not match expected package and/or "
                "version.\n",
                path.c_str());

        err = UNKNOWN_ERROR;
    } else {
        std::string ifaceName;
        if (ast->isInterface(&ifaceName)) {
            if (fqName.name() == "types") {
                fprintf(stderr,
                        "ERROR: File at '%s' declares an interface '%s' "
                        "instead of the expected types common to the package.\n",
                        path.c_str(),
                        ifaceName.c_str());

                err = UNKNOWN_ERROR;
            } else if (ifaceName != fqName.name()) {
                fprintf(stderr,
                        "ERROR: File at '%s' does not declare interface type "
                        "'%s'.\n",
                        path.c_str(),
                        fqName.name().c_str());

                err = UNKNOWN_ERROR;
            }
        } else if (fqName.name() != "types") {
            fprintf(stderr,
                    "ERROR: File at '%s' declares types rather than the "
                    "expected interface type '%s'.\n",
                    path.c_str(),
                    fqName.name().c_str());

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

std::vector<std::string>::const_iterator
Coordinator::findPackageRoot(const FQName &fqName) const {
    CHECK(!fqName.package().empty());
    CHECK(!fqName.version().empty());

    // Find the right package prefix and path for this FQName.  For
    // example, if FQName is "android.hardware.nfc@1.0::INfc", and the
    // prefix:root is set to [ "android.hardware:hardware/interfaces",
    // "vendor.qcom.hardware:vendor/qcom"], then we will identify the
    // prefix "android.hardware" and the package root
    // "hardware/interfaces".

    // TODO: This now returns on the first match.  Throw an error if
    // there are multiple hits.
    auto it = mPackageRoots.begin();
    for (; it != mPackageRoots.end(); it++) {
        if (fqName.package().find(*it) != std::string::npos) {
            break;
        }
    }
    CHECK(it != mPackageRoots.end());

    return it;
}

std::string Coordinator::getPackageRoot(const FQName &fqName) const {
    auto it = findPackageRoot(fqName);
    auto prefix = *it;
    return prefix;
}

std::string Coordinator::getPackageRootPath(const FQName &fqName) const {
    auto it = findPackageRoot(fqName);
    auto root = mPackageRootPaths[std::distance(mPackageRoots.begin(), it)];
    return root;
}

std::string Coordinator::getPackagePath(
        const FQName &fqName, bool relative) const {

    auto it = findPackageRoot(fqName);
    auto prefix = *it;
    auto root = mPackageRootPaths[std::distance(mPackageRoots.begin(), it)];

    // Make sure the prefix ends on a '.' and the root path on a '/'
    if ((*--prefix.end()) != '.') {
        prefix += '.';
    }

    if ((*--root.end()) != '/') {
        root += '/';
    }

    // Given FQName of "android.hardware.nfc@1.0::IFoo" and a prefix
    // "android.hardware.", the suffix is "nfc@1.0::IFoo".
    const std::string packageSuffix = fqName.package().substr(prefix.length());

    std::string packagePath;
    if (!relative) {
        packagePath = root;
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
            return type->ref();
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
                return type->ref();
            }
        }
    }

    return NULL;
}

status_t Coordinator::getPackageInterfaceFiles(
        const FQName &package,
        std::vector<std::string> *fileNames) const {
    fileNames->clear();

    const std::string packagePath = getPackagePath(package);

    DIR *dir = opendir(packagePath.c_str());

    if (dir == NULL) {
        return -errno;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }

        const auto suffix = ".hal";
        const auto suffix_len = std::strlen(suffix);
        const auto d_namelen = strlen(ent->d_name);

        if (d_namelen < suffix_len
                || strcmp(ent->d_name + d_namelen - suffix_len, suffix)) {
            continue;
        }

        fileNames->push_back(std::string(ent->d_name, d_namelen - suffix_len));
    }

    closedir(dir);
    dir = NULL;

    return OK;
}

status_t Coordinator::appendPackageInterfacesToSet(
        const FQName &package,
        std::vector<FQName> *packageInterfaces) const {
    packageInterfaces->clear();

    std::vector<std::string> fileNames;
    status_t err = getPackageInterfaceFiles(package, &fileNames);

    if (err != OK) {
        return err;
    }

    for (const auto &fileName : fileNames) {
        FQName subFQName(
                package.package() + package.version() + "::" + fileName);

        if (!subFQName.isValid()) {
            LOG(WARNING)
                << "Whole-package import encountered invalid filename '"
                << fileName
                << "' in package "
                << package.package()
                << package.version();

            continue;
        }

        packageInterfaces->push_back(subFQName);
    }

    return OK;
}

std::string Coordinator::convertPackageRootToPath(const FQName &fqName) const {
    std::string packageRoot = getPackageRoot(fqName);

    if (*(packageRoot.end()--) != '.') {
        packageRoot += '.';
    }

    std::replace(packageRoot.begin(), packageRoot.end(), '.', '/');

    return packageRoot; // now converted to a path
}

// static
bool Coordinator::MakeParentHierarchy(const std::string &path) {
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

}  // namespace android

