#ifndef AST_H_

#define AST_H_

#include <android-base/macros.h>
#include <set>
#include <string>
#include <utils/Vector.h>

#include "FQName.h"
#include "Type.h"

namespace android {

struct Coordinator;
struct Formatter;
struct Method;
struct TypedVar;
struct Scope;

struct AST {
    AST(Coordinator *coordinator);
    ~AST();

    bool setPackage(const char *package);
    bool addImport(const char *import);

    // package and version really.
    FQName package() const;
    bool isInterface(std::string *ifaceName) const;

    void enterScope(Scope *container);
    void leaveScope();
    Scope *scope();

    void *scanner();
    void setScanner(void *scanner);

    // Look up a type by FQName, "pure" names, i.e. those without package
    // or version are first looked up in the current scope chain.
    // After that lookup proceeds to imports.
    Type *lookupType(const char *name);

    // Takes dot-separated path components to a type possibly inside this AST.
    // Name resolution goes from root scope downwards, i.e. the path must be
    // absolute.
    Type *lookupTypeInternal(const std::string &namePath) const;

    void dump(Formatter &out) const;

    status_t generateCpp(const std::string &outputPath) const;

private:
    Coordinator *mCoordinator;
    Vector<Scope *> mScopePath;

    void *mScanner;
    Scope *mRootScope;

    FQName mPackage;

    std::set<FQName> mImportedNames;

    static void GetPackageComponents(
            const FQName &fqName, std::vector<std::string> *components);

    static void GetPackageAndVersionComponents(
            const FQName &fqName,
            std::vector<std::string> *components,
            bool cpp_compatible);

    void getPackageComponents(std::vector<std::string> *components) const;

    void getPackageAndVersionComponents(
            std::vector<std::string> *components, bool cpp_compatible) const;

    std::string makeHeaderGuard(const std::string &baseName) const;
    void enterLeaveNamespace(Formatter &out, bool enter) const;

    status_t generateInterfaceHeader(const std::string &outputPath) const;
    status_t generateStubHeader(const std::string &outputPath) const;
    status_t generateProxyHeader(const std::string &outputPath) const;
    status_t generateAllSource(const std::string &outputPath) const;

    status_t generateTypeSource(
            Formatter &out, const std::string &ifaceName) const;

    status_t generateProxySource(
            Formatter &out, const std::string &baseName) const;

    status_t generateStubSource(
            Formatter &out, const std::string &baseName) const;

    status_t generateStubSourceForMethod(
            Formatter &out, const Method *method) const;

    void emitCppReaderWriter(
            Formatter &out,
            const std::string &parcelObj,
            bool parcelObjIsPointer,
            const TypedVar *arg,
            bool isReader,
            Type::ErrorMode mode) const;

    status_t emitTypeDeclarations(Formatter &out) const;

    DISALLOW_COPY_AND_ASSIGN(AST);
};

}  // namespace android

#endif  // AST_H_
