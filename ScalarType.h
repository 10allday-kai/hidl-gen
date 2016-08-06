#ifndef SCALAR_TYPE_H_

#define SCALAR_TYPE_H_

#include "Type.h"

namespace android {

struct ScalarType : public Type {
    enum Kind {
        KIND_CHAR,
        KIND_BOOL,
        KIND_OPAQUE,
        KIND_INT8,
        KIND_UINT8,
        KIND_INT16,
        KIND_UINT16,
        KIND_INT32,
        KIND_UINT32,
        KIND_INT64,
        KIND_UINT64,
        KIND_FLOAT,
        KIND_DOUBLE,
    };

    ScalarType(Kind kind);

    void dump(Formatter &out) const override;

    const ScalarType *resolveToScalarType() const override;

    std::string getCppType(StorageMode mode, std::string *extra) const override;

    void emitReaderWriter(
            Formatter &out,
            const std::string &name,
            const std::string &parcelObj,
            bool parcelObjIsPointer,
            bool isReader,
            ErrorMode mode) const override;

    void emitReaderWriterWithCast(
            Formatter &out,
            const std::string &name,
            const std::string &parcelObj,
            bool parcelObjIsPointer,
            bool isReader,
            ErrorMode mode,
            bool needsCast) const;

private:
    Kind mKind;

    DISALLOW_COPY_AND_ASSIGN(ScalarType);
};

}  // namespace android

#endif  // SCALAR_TYPE_H_
