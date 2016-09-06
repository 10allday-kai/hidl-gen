#ifndef HIDL_GENERATED_android_hardware_tests_expression_V1_0_BpExpression_H_
#define HIDL_GENERATED_android_hardware_tests_expression_V1_0_BpExpression_H_

#include <android/hardware/tests/expression/1.0/IHwExpression.h>

namespace android {
namespace hardware {
namespace tests {
namespace expression {
namespace V1_0 {

struct BpExpression : public ::android::hardware::BpInterface<IHwExpression> {
  explicit BpExpression(const ::android::sp<::android::hardware::IBinder> &_hidl_impl);

  virtual bool isRemote() const { return true; }

  // Methods from IExpression follow.

};

}  // namespace V1_0
}  // namespace expression
}  // namespace tests
}  // namespace hardware
}  // namespace android

#endif  // HIDL_GENERATED_android_hardware_tests_expression_V1_0_BpExpression_H_
