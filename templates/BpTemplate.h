#define param_list ::android::String16* _aidl_return
// SECTION bp_h
// START file
// AUTO_GENERATED FILE - DO NOT EDIT
// see system/tools/hidl/templates/BpTemplate.h
#ifndef HIDL_GENERATED_namespace_underscores_BP_header_guard_H_
#define HIDL_GENERATED_namespace_underscores_BP_header_guard_H_

#include <hwbinder/IBinder.h>
#include <hwbinder/IInterface.h>
#include <utils/Errors.h>
//#include <namespace_slashes/Ipackage_name.h>
#include <Ipackage_name.h>

// START namespace_open_section
namespace namespace_name {//ALL namespace_open_line
//END namespace_open_section

class Bppackage_name : public ::android::hardware::BpInterface<Ipackage_name> {
public:
explicit Bppackage_name(const ::android::sp<::android::hardware::IBinder>& _aidl_impl);
virtual ~Bppackage_name() = default;
  // START declarations
/*
Ipackage_name::function_name_cb _cb // ALL callback_param // Used in callback_description
sp<import_name> param_name // ALL param_decl_import
const hidl_vec<decl_base_type> &param_name // ALL param_decl_vec
const Ipackage_name::struct_name &param_name // ALL param_decl_struct_type
*/
::android::hardware::Status function_name(params_and_callback) override; // ALL declare_function
  // END declarations
};  // class Bppackage_name

// START namespace_close_section
}  // namespace namespace_name  //ALL namespace_close_line
//END namespace_close_section

#endif  // HIDL_GENERATED_namespace_underscores_BP_header_guard_H_
// END file
