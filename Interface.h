#ifndef INTERFACE_H_

#define INTERFACE_H_

#include "Method.h"
#include "Scope.h"

#include <utils/KeyedVector.h>
#include <vector>

namespace android {

struct Annotation;
struct Method;

struct Interface : public Scope {
    Interface(
            Interface *super,
            AnnotationVector *annotations);

    void addMethod(Method *method);

    bool isInterface() const override;

    const Interface *superType() const;

    const std::vector<Method *> &methods() const;

    const AnnotationVector &annotations() const;

    std::string getCppType(StorageMode mode, std::string *extra) const override;

    std::string getJavaType() const override;

    void emitReaderWriter(
            Formatter &out,
            const std::string &name,
            const std::string &parcelObj,
            bool parcelObjIsPointer,
            bool isReader,
            ErrorMode mode) const override;

    void emitJavaReaderWriter(
            Formatter &out,
            const std::string &parcelObj,
            const std::string &argName,
            bool isReader) const override;

    status_t emitVtsArgumentType(Formatter &out) const override;

    bool isJavaCompatible() const override;

private:
    Interface *mSuperType;
    std::vector<Method *> mMethods;
    AnnotationVector *mAnnotationsByName;

    DISALLOW_COPY_AND_ASSIGN(Interface);
};

}  // namespace android

#endif  // INTERFACE_H_

