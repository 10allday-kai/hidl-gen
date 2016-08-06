#ifndef COORDINATOR_H_

#define COORDINATOR_H_

#include <android-base/macros.h>
#include <functional>
#include <string>
#include <utils/KeyedVector.h>
#include <vector>

namespace android {

struct AST;
struct FQName;
struct Type;

struct Coordinator {
    Coordinator(
            const std::vector<std::string> &packageRootPaths,
            const std::vector<std::string> &packageRoots);

    ~Coordinator();

    AST *parse(const FQName &fqName);

    Type *lookupType(const FQName &fqName) const;

    // Given package-root paths of ["hardware/interfaces",
    // "vendor/<something>/interfaces"], package roots of
    // ["android.hardware", "vendor.<something>.hardware"], and a
    // FQName of "android.hardware.nfc@1.0::INfc, then getPackagePath()
    // will return "harware/interfaces/nfc/V1_0".

    std::string getPackagePath(
            const FQName &fqName, bool relative = false) const;

    // Given package roots of ["android.hardware",
    // "vendor.<something>.hardware"] and a FQName of
    // "android.hardware.nfc@1.0::INfc, the getPackageRoot() will
    // return "android.hardware".

    std::string getPackageRoot(const FQName &fqName) const;

    // Iterate over all cached ASTs and abort if any callback returns an error.
    using for_each_cb = std::function<status_t(const AST *ast)>;
    status_t forEachAST(for_each_cb cb) const;

private:
    // A list of top-level directories (mPackageRootPaths)
    // corresponding to a list of package roots (mPackageRoots). For
    // example, if mPackageRootPaths[0] == "hardware/interfaces" and
    // mPackageRoots[0] == "android.hardware" this means that all
    // packages starting with "android.hardware" will be looked up in
    // "hardware/interfaces".
    std::vector<std::string> mPackageRootPaths;
    std::vector<std::string> mPackageRoots;
    KeyedVector<FQName, AST *> mCache;

    std::vector<std::string>::const_iterator findPackageRoot(
            const FQName &fqName) const;

    DISALLOW_COPY_AND_ASSIGN(Coordinator);
};

}  // namespace android

#endif  // COORDINATOR_H_
