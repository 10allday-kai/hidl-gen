#define param_list ::android::String16* _aidl_return
// SECTION bp_h
// START file
// AUTO_GENERATED FILE - DO NOT EDIT
// see system/tools/hidl/templates/BpTemplate.h
#ifndef HIDL_GENERATED_ANDROID_HARDWARE_TESTS_BP_header_guard_H_
#define HIDL_GENERATED_ANDROID_HARDWARE_TESTS_BP_header_guard_H_

#include <hwbinder/IBinder.h>
#include <hwbinder/IInterface.h>
#include <utils/Errors.h>
#include <android/hardware/tests/Ipackage_name.h>

// START namespace_open_section
namespace namespace_name {//ALL namespace_open_line
//END namespace_open_section

class Bppackage_name : public ::android::BpInterface<Ipackage_name> {
public:
explicit Bppackage_name(const ::android::sp<::android::IBinder>& _aidl_impl);
virtual ~Bppackage_name() = default;
  // START declarations
::android::binder::Status function_name(call_param_list return_param_list) override; // ALL declare_function
  // END declarations
};  // class Bppackage_name

// START namespace_close_section
}  // namespace namespace_name  //ALL namespace_close_line
//END namespace_close_section

#endif  // HIDL_GENERATED_ANDROID_HARDWARE_TESTS_BP_header_guard_H_
// END file
