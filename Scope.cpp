#include "Scope.h"

#include "Constant.h"
#include "Formatter.h"
#include "Interface.h"

namespace android {

Scope::Scope() {}

bool Scope::addType(const char *localName, NamedType *type) {
    if (mTypeIndexByName.indexOfKey(localName) >= 0) {
        return false;
    }

    size_t index = mTypes.size();
    mTypes.push_back(type);
    mTypeIndexByName.add(localName, index);

    return true;
}

bool Scope::addConstant(Constant *constant) {
    ssize_t index = mConstants.indexOfKey(constant->name());

    if (index >= 0) {
        return false;
    }

    mConstants.add(constant->name(), constant);

    return true;
}

Type *Scope::lookupType(const char *name) const {
    ssize_t index = mTypeIndexByName.indexOfKey(name);

    if (index >= 0) {
        return mTypes[mTypeIndexByName.valueAt(index)];
    }

    return NULL;
}

bool Scope::isScope() const {
    return true;
}

Interface *Scope::getInterface() const {
    if (mTypes.size() == 1 && mTypes[0]->isInterface()) {
        return static_cast<Interface *>(mTypes[0]);
    }

    return NULL;
}

bool Scope::containsSingleInterface(std::string *ifaceName) const {
    Interface *iface = getInterface();

    if (iface != NULL) {
        *ifaceName = iface->localName();
        return true;
    }

    return false;
}

status_t Scope::emitTypeDeclarations(Formatter &out) const {
    for (size_t i = 0; i < mTypes.size(); ++i) {
        status_t err = mTypes[i]->emitTypeDeclarations(out);

        if (err != OK) {
            return err;
        }
    }

    return OK;
}

status_t Scope::emitTypeDefinitions(
        Formatter &out, const std::string prefix) const {
    for (size_t i = 0; i < mTypes.size(); ++i) {
        status_t err = mTypes[i]->emitTypeDefinitions(out, prefix);

        if (err != OK) {
            return err;
        }
    }

    return OK;
}

}  // namespace android

