#define LOG_TAG "hidl_test"
#include <android-base/logging.h>

#include <android/hardware/tests/foo/1.0/BnFoo.h>
#include <android/hardware/tests/foo/1.0/BnFooCallback.h>
#include <android/hardware/tests/bar/1.0/BnBar.h>

#include <gtest/gtest.h>
#include <inttypes.h>
#if GTEST_IS_THREADSAFE
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#else
#error "GTest did not detect pthread library."
#endif

#include <hidl/IServiceManager.h>
#include <hidl/Status.h>
#include <hwbinder/IPCThreadState.h>
#include <hwbinder/ProcessState.h>

#include <utils/Condition.h>
#include <utils/Timers.h>

using ::android::hardware::tests::foo::V1_0::BnFoo;
using ::android::hardware::tests::foo::V1_0::BnFooCallback;
using ::android::hardware::tests::bar::V1_0::BnBar;
using ::android::hardware::tests::foo::V1_0::IFoo;
using ::android::hardware::tests::foo::V1_0::IFooCallback;
using ::android::hardware::tests::bar::V1_0::IBar;
using ::android::hardware::tests::bar::V1_0::IHwBar;
using ::android::hardware::tests::foo::V1_0::Abc;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;
using ::android::Mutex;
using ::android::Condition;

struct FooCallback : public IFooCallback {
    FooCallback() : invokeInfo{}, mLock{}, mCond{} {}
    Return<void> heyItsYou(const sp<IFooCallback> &cb) override;
    Return<bool> heyItsYouIsntIt(const sp<IFooCallback> &cb) override;
    Return<void> heyItsTheMeaningOfLife(uint8_t tmol) override;
    Return<void> reportResults(int64_t ns, reportResults_cb cb) override;
    Return<void> youBlockedMeFor(const int64_t ns[3]) override;

    static constexpr nsecs_t DELAY_S = 1;
    static constexpr nsecs_t DELAY_NS = seconds_to_nanoseconds(DELAY_S);
    static constexpr nsecs_t TOLERANCE_NS = milliseconds_to_nanoseconds(10);
    static constexpr nsecs_t ONEWAY_TOLERANCE_NS = milliseconds_to_nanoseconds(1);

    InvokeInfo invokeInfo[3];
    Mutex mLock;
    Condition mCond;
};

Return<void> FooCallback::heyItsYou(
        const sp<IFooCallback> &_cb) {
    nsecs_t start = systemTime();
    ALOGI("SERVER(FooCallback) heyItsYou cb = %p", _cb.get());
    mLock.lock();
    invokeInfo[0].invoked = true;
    invokeInfo[0].timeNs = systemTime() - start;
    mCond.signal();
    mLock.unlock();
    return Void();
}

Return<bool> FooCallback::heyItsYouIsntIt(const sp<IFooCallback> &_cb) {
    nsecs_t start = systemTime();
    ALOGI("SERVER(FooCallback) heyItsYouIsntIt cb = %p sleeping for %" PRId64 " seconds", _cb.get(), DELAY_S);
    sleep(DELAY_S);
    ALOGI("SERVER(FooCallback) heyItsYouIsntIt cb = %p responding", _cb.get());
    mLock.lock();
    invokeInfo[1].invoked = true;
    invokeInfo[1].timeNs = systemTime() - start;
    mCond.signal();
    mLock.unlock();
    return true;
}

Return<void> FooCallback::heyItsTheMeaningOfLife(uint8_t tmol) {
    nsecs_t start = systemTime();
    ALOGI("SERVER(FooCallback) heyItsTheMeaningOfLife = %d sleeping for %" PRId64 " seconds", tmol, DELAY_S);
    sleep(DELAY_S);
    ALOGI("SERVER(FooCallback) heyItsTheMeaningOfLife = %d done sleeping", tmol);
    mLock.lock();
    invokeInfo[2].invoked = true;
    invokeInfo[2].timeNs = systemTime() - start;
    mCond.signal();
    mLock.unlock();
    return Void();
}

Return<void> FooCallback::reportResults(int64_t ns, reportResults_cb cb) {
    ALOGI("SERVER(FooCallback) reportResults(%" PRId64 " seconds)", nanoseconds_to_seconds(ns));
    nsecs_t leftToWaitNs = ns;
    mLock.lock();
    while (!(invokeInfo[0].invoked && invokeInfo[1].invoked && invokeInfo[2].invoked) &&
           leftToWaitNs > 0) {
      nsecs_t start = systemTime();
      ::android::status_t rc = mCond.waitRelative(mLock, leftToWaitNs);
      if (rc != ::android::OK) {
          ALOGI("SERVER(FooCallback)::reportResults(%" PRId64 " ns) Condition::waitRelative(%" PRId64 ") returned error (%d)", ns, leftToWaitNs, rc);
          break;
      }
      ALOGI("SERVER(FooCallback)::reportResults(%" PRId64 " ns) Condition::waitRelative was signalled", ns);
      leftToWaitNs -= systemTime() - start;
    }
    mLock.unlock();
    cb(leftToWaitNs, invokeInfo);
    return Void();
}

Return<void> FooCallback::youBlockedMeFor(const int64_t ns[3]) {
    for (size_t i = 0; i < 3; i++) {
        invokeInfo[i].callerBlockedNs = ns[i];
    }
    return Void();
}

struct Bar : public IBar {
    Return<void> doThis(float param) override;

    Return<int32_t> doThatAndReturnSomething(int64_t param) override;

    Return<double> doQuiteABit(
            int32_t a,
            int64_t b,
            float c,
            double d) override;

    Return<void> doSomethingElse(
            const int32_t param[15], doSomethingElse_cb _cb) override;

    Return<void> doStuffAndReturnAString(
            doStuffAndReturnAString_cb _cb) override;

    Return<void> mapThisVector(
            const hidl_vec<int32_t> &param, mapThisVector_cb _cb) override;

    Return<void> callMe(
            const sp<IFooCallback> &cb) override;

    Return<SomeEnum> useAnEnum(SomeEnum param) override;

    Return<void> haveAGooberVec(const hidl_vec<Goober>& param) override;
    Return<void> haveAGoober(const Goober &g) override;
    Return<void> haveAGooberArray(const Goober lots[20]) override;

    Return<void> haveATypeFromAnotherFile(const Abc &def) override;

    Return<void> haveSomeStrings(
            const hidl_string array[3],
            haveSomeStrings_cb _cb) override;

    Return<void> haveAStringVec(
            const hidl_vec<hidl_string> &vector,
            haveAStringVec_cb _cb) override;

    Return<void> transposeMe(
            const float *in /* float[3][5] */, transposeMe_cb _cb) override;

    Return<void> callingDrWho(
            const MultiDimensional &in,
            callingDrWho_cb _hidl_cb) override;

    Return<void> thisIsNew() override;
};

Return<void> Bar::doThis(float param) {
    ALOGI("SERVER(Bar) doThis(%.2f)", param);

    return Void();
}

Return<int32_t> Bar::doThatAndReturnSomething(
        int64_t param) {
    ALOGI("SERVER(Bar) doThatAndReturnSomething(%ld)", param);

    return 666;
}

Return<double> Bar::doQuiteABit(
        int32_t a,
        int64_t b,
        float c,
        double d) {
    ALOGI("SERVER(Bar) doQuiteABit(%d, %ld, %.2f, %.2f)", a, b, c, d);

    return 666.5;
}

Return<void> Bar::doSomethingElse(
        const int32_t param[15], doSomethingElse_cb _cb) {
    ALOGI("SERVER(Bar) doSomethingElse(...)");

    int32_t result[32] = { 0 };
    for (size_t i = 0; i < 15; ++i) {
        result[i] = 2 * param[i];
        result[15 + i] = param[i];
    }
    result[30] = 1;
    result[31] = 2;

    _cb(result);

    return Void();
}

Return<void> Bar::doStuffAndReturnAString(
        doStuffAndReturnAString_cb _cb) {
    ALOGI("SERVER(Bar) doStuffAndReturnAString");

    hidl_string s;
    s = "Hello, world";

    _cb(s);

    return Void();
}

Return<void> Bar::mapThisVector(
        const hidl_vec<int32_t> &param, mapThisVector_cb _cb) {
    ALOGI("SERVER(Bar) mapThisVector");

    hidl_vec<int32_t> out;
    out.resize(param.size());

    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = param[i] * 2;
    }

    _cb(out);

    return Void();
}

Return<void> Bar::callMe(
        const sp<IFooCallback> &cb) {
    ALOGI("SERVER(Bar) callMe %p", cb.get());

    if (cb != NULL) {

        nsecs_t c[3];
        ALOGI("SERVER(Bar) callMe %p calling IFooCallback::heyItsYou, " \
              "should return immediately", cb.get());
        c[0] = systemTime();
        cb->heyItsYou(cb);
        c[0] = systemTime() - c[0];
        ALOGI("SERVER(Bar) callMe %p calling IFooCallback::heyItsYou " \
              "returned after %" PRId64 "ns", cb.get(), c[0]);

        ALOGI("SERVER(Bar) callMe %p calling IFooCallback::heyItsYouIsntIt, " \
              "should block for %" PRId64 " seconds", cb.get(),
              FooCallback::DELAY_S);
        c[1] = systemTime();
        bool answer = cb->heyItsYouIsntIt(cb);
        c[1] = systemTime() - c[1];
        ALOGI("SERVER(Bar) callMe %p IFooCallback::heyItsYouIsntIt " \
              "responded with %d after %" PRId64 "ns", cb.get(), answer, c[1]);

        ALOGI("SERVER(Bar) callMe %p calling " \
              "IFooCallback::heyItsTheMeaningOfLife, " \
              "should return immediately ", cb.get());
        c[2] = systemTime();
        cb->heyItsTheMeaningOfLife(42);
        c[2] = systemTime() - c[2];
        ALOGI("SERVER(Bar) callMe %p After call to " \
              "IFooCallback::heyItsTheMeaningOfLife " \
              "responded after %" PRId64 "ns", cb.get(), c[2]);

        ALOGI("SERVER(Bar) callMe %p calling IFooCallback::youBlockedMeFor " \
              "to report times", cb.get());
        cb->youBlockedMeFor(c);
        ALOGI("SERVER(Bar) callMe %p After call to " \
              "IFooCallback::heyYouBlockedMeFor", cb.get());
    }

    return Void();
}

Return<Bar::SomeEnum> Bar::useAnEnum(SomeEnum param) {
    ALOGI("SERVER(Bar) useAnEnum %d", (int)param);

    return SomeEnum::goober;
}

Return<void> Bar::haveAGooberVec(const hidl_vec<Goober>& param) {
    ALOGI("SERVER(Bar) haveAGooberVec &param = %p", &param);

    return Void();
}

Return<void> Bar::haveAGoober(const Goober &g) {
    ALOGI("SERVER(Bar) haveaGoober g=%p", &g);

    return Void();
}

Return<void> Bar::haveAGooberArray(const Goober lots[20]) {
    ALOGI("SERVER(Bar) haveAGooberArray lots = %p", lots);

    return Void();
}

Return<void> Bar::haveATypeFromAnotherFile(const Abc &def) {
    ALOGI("SERVER(Bar) haveATypeFromAnotherFile def=%p", &def);

    return Void();
}

Return<void> Bar::haveSomeStrings(
        const hidl_string array[3],
        haveSomeStrings_cb _cb) {
    ALOGI("SERVER(Bar) haveSomeStrings([\"%s\", \"%s\", \"%s\"])",
          array[0].c_str(),
          array[1].c_str(),
          array[2].c_str());

    hidl_string result[2];
    result[0] = "Hello";
    result[1] = "World";

    _cb(result);

    return Void();
}

Return<void> Bar::haveAStringVec(
        const hidl_vec<hidl_string> &vector,
        haveAStringVec_cb _cb) {
    ALOGI("SERVER(Bar) haveAStringVec([\"%s\", \"%s\", \"%s\"])",
          vector[0].c_str(),
          vector[1].c_str(),
          vector[2].c_str());

    hidl_vec<hidl_string> result;
    result.resize(2);

    result[0] = "Hello";
    result[1] = "World";

    _cb(result);

    return Void();
}

static std::string FloatArray2DToString(const float *x, size_t n1, size_t n2) {
    std::string s;
    s += "[";
    for (size_t i = 0; i < n1; ++i) {
        if (i > 0) {
            s += ", ";
        }

        s += "[";
        for (size_t j = 0; j < n2; ++j) {
            if (j > 0) {
                s += ", ";
            }
            s += std::to_string(x[i * n2 + j]);
        }
        s += "]";
    }
    s += "]";

    return s;
}

Return<void> Bar::transposeMe(
        const float *in /* float[3][5] */, transposeMe_cb _cb) {
    ALOGI("SERVER(Bar) transposeMe(%s)",
          FloatArray2DToString(in, 3, 5).c_str());

    float out[5][3];
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            out[i][j] = in[5 * j + i];
        }
    }

    _cb(&out[0][0]);

    return Void();
}

static std::string QuuxToString(const IFoo::Quux &val) {
    std::string s;

    s = "Quux(first='";
    s += val.first.c_str();
    s += "', last='";
    s += val.last.c_str();
    s += "')";

    return s;
}

static std::string MultiDimensionalToString(const IFoo::MultiDimensional &val) {
    std::string s;

    s += "MultiDimensional(";

    s += "quuxMatrix=[";
    for (size_t i = 0; i < 5; ++i) {
        if (i > 0) {
            s += ", ";
        }

        s += "[";
        for (size_t j = 0; j < 3; ++j) {
            if (j > 0) {
                s += ", ";
            }

            s += QuuxToString(val.quuxMatrix[i][j]);
        }
    }
    s += "]";

    s += ")";

    return s;
}

Return<void> Bar::callingDrWho(
        const MultiDimensional &in, callingDrWho_cb _hidl_cb) {
    ALOGI("SERVER(Bar) callingDrWho(%s)", MultiDimensionalToString(in).c_str());

    MultiDimensional out;
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            out.quuxMatrix[i][j].first = in.quuxMatrix[4 - i][2 - j].last;
            out.quuxMatrix[i][j].last = in.quuxMatrix[4 - i][2 - j].first;
        }
    }

    _hidl_cb(out);

    return Void();
}

Return<void> Bar::thisIsNew() {
    ALOGI("SERVER(Bar) thisIsNew");

    return Void();
}

template<typename I>
static std::string arraylikeToString(const I data, size_t size) {
    std::string out = "[";
    for (size_t i = 0; i < size; ++i) {
        if (i > 0) {
            out += ", ";
        }

        out += ::android::String8::format("%d", data[i]).string();
    }
    out += "]";

    return out;
}


static std::string vecToString(const hidl_vec<int32_t> &vec) {
    return arraylikeToString(vec, vec.size());
}

#define EXPECT_OK(ret) EXPECT_TRUE(ret.getStatus().isOk())

template<typename T, typename S>
static inline bool isArrayEqual(const T arr1, const S arr2, size_t size) {
    for(size_t i = 0; i < size; i++)
        if(arr1[i] != arr2[i])
            return false;
    return true;
}


template <class T>
static void startServer(T server,
                        const char *serviceName,
                        const char *tag) {
    using namespace android::hardware;
    ALOGI("SERVER(%s) registering", tag);
    server->registerAsService(serviceName);
    ALOGI("SERVER(%s) starting", tag);
    ProcessState::self()->setThreadPoolMaxThreadCount(0);
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool(); // never ends. needs kill().
    ALOGI("SERVER(%s) ends.", tag);
}


class HidlTest : public ::testing::Test {
public:
    sp<::android::hardware::IBinder> service;
    sp<IFoo> foo;
    sp<IBar> bar;
    sp<IFooCallback> fooCb;
    sp<::android::hardware::IBinder> cbService;
    virtual void SetUp() override {
        ALOGI("Test setup beginning...");
        using namespace android::hardware;
        foo = IFoo::getService("foo");
        CHECK(foo != NULL);

        bar = IBar::getService("foo");
        CHECK(bar != NULL);

        fooCb = IFooCallback::getService("foo callback");
        CHECK(fooCb != NULL);

        ALOGI("Test setup complete");
    }
    virtual void TearDown() override {
    }
};

class HidlEnvironment : public ::testing::Environment {
private:
    pid_t fooCallbackServerPid, barServerPid;
public:
    virtual void SetUp() {
        ALOGI("Environment setup beginning...");
        // use fork to create and kill to destroy server processes.
        if ((barServerPid = fork()) == 0) {
            // Fear me, I am a child.
            startServer(new Bar, "foo", "Bar"); // never returns
            return;
        }

        if ((fooCallbackServerPid = fork()) == 0) {
            // Fear me, I am a second child.
            startServer(new FooCallback, "foo callback", "FooCalback"); // never returns
            return;
        }

        // Fear you not, I am parent.
        sleep(1);
        ALOGI("Environment setup complete.");
    }

    virtual void TearDown() {
        // clean up by killing server processes.
        ALOGI("Environment tear-down beginning...");
        ALOGI("Killing servers...");
        if(kill(barServerPid, SIGTERM)) {
            ALOGE("Could not kill barServer; errno = %d", errno);
        } else {
            int status;
            ALOGI("Waiting for barServer to exit...");
            waitpid(barServerPid, &status, 0);
            ALOGI("Continuing...");
        }
        if(kill(fooCallbackServerPid, SIGTERM)) {
            ALOGE("Could not kill fooCallbackServer; errno = %d", errno);
        } else {
            int status;
            ALOGI("Waiting for fooCallbackServer to exit...");
            waitpid(barServerPid, &status, 0);
            ALOGI("Continuing...");
        }
        ALOGI("Servers all killed.");
        ALOGI("Environment tear-down complete.");
    }
};

TEST_F(HidlTest, FooDoThisTest) {
    ALOGI("CLIENT call doThis.");
    EXPECT_OK(foo->doThis(1.0f));
    ALOGI("CLIENT doThis returned.");
    EXPECT_EQ(true, true);
}

TEST_F(HidlTest, FooDoThatAndReturnSomethingTest) {
    ALOGI("CLIENT call doThatAndReturnSomething.");
    int32_t result = foo->doThatAndReturnSomething(2.0f);
    ALOGI("CLIENT doThatAndReturnSomething returned %d.", result);
    EXPECT_EQ(result, 666);
}

TEST_F(HidlTest, FooDoQuiteABitTest) {
    ALOGI("CLIENT call doQuiteABit");
    double something = foo->doQuiteABit(1, 2, 3.0f, 4.0);
    ALOGI("CLIENT doQuiteABit returned %f.", something);
    EXPECT_DOUBLE_EQ(something, 666.5);
}

TEST_F(HidlTest, FooDoSomethingElseTest) {

    ALOGI("CLIENT call doSomethingElse");
    int32_t param[15];
    for (size_t i = 0; i < sizeof(param) / sizeof(param[0]); ++i) {
        param[i] = i;
    }
    EXPECT_OK(foo->doSomethingElse(param, [&](const auto &something) {
            ALOGI("CLIENT doSomethingElse returned %s.",
                  arraylikeToString(something, 32).c_str());
            int32_t expect[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24,
                26, 28, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2};
            EXPECT_TRUE(isArrayEqual(something, expect, 32));
        }));
}

TEST_F(HidlTest, FooDoStuffAndReturnAStringTest) {
    ALOGI("CLIENT call doStuffAndReturnAString");
    EXPECT_OK(foo->doStuffAndReturnAString([&](const auto &something) {
            ALOGI("CLIENT doStuffAndReturnAString returned '%s'.",
                  something.c_str());
            EXPECT_STREQ(something.c_str(), "Hello, world");
        }));
}

TEST_F(HidlTest, FooMapThisVectorTest) {
    hidl_vec<int32_t> vecParam;
    vecParam.resize(10);
    for (size_t i = 0; i < 10; ++i) {
        vecParam[i] = i;
    }
    EXPECT_OK(foo->mapThisVector(vecParam, [&](const auto &something) {
            ALOGI("CLIENT mapThisVector returned %s.",
                  vecToString(something).c_str());
            int32_t expect[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
            EXPECT_TRUE(isArrayEqual(something, expect, something.size()));
        }));
}

TEST_F(HidlTest, FooCallMeTest) {
    ALOGI("CLIENT call callMe.");
    // callMe is oneway, should return instantly.
    nsecs_t now;
    now = systemTime();
    EXPECT_OK(foo->callMe(fooCb));
    EXPECT_TRUE(systemTime() - now < FooCallback::ONEWAY_TOLERANCE_NS);
    ALOGI("CLIENT callMe returned.");
}

TEST_F(HidlTest, ForReportResultsTest) {

    // Bar::callMe will invoke three methods on FooCallback; one will return
    // right away (even though it is a two-way method); the second one will
    // block Bar for FooCallback::DELAY_S seconds, and the third one will return
    // to Bar right away (is oneway) but will itself block for DELAY_S seconds.
    // We need a way to make sure that these three things have happened within
    // 2*DELAY_S seconds plus some small tolerance.
    //
    // Method FooCallback::reportResults() takes a timeout parameter.  It blocks for
    // that length of time, while waiting for the three methods above to
    // complete.  It returns the information of whether each method was invoked,
    // as well as how long the body of the method took to execute.  We verify
    // the information returned by reportResults() against the timeout we pass (which
    // is long enough for the method bodies to execute, plus tolerance), and
    // verify that eachof them executed, as expected, and took the length of
    // time to execute that we also expect.

    const nsecs_t reportResultsNs =
        2 * FooCallback::DELAY_NS + FooCallback::TOLERANCE_NS;

    ALOGI("CLIENT: Waiting for up to %" PRId64 " seconds.",
          nanoseconds_to_seconds(reportResultsNs));

    fooCb->reportResults(reportResultsNs,
                [&](int64_t timeLeftNs,
                    const IFooCallback::InvokeInfo invokeResults[3]) {
        ALOGI("CLIENT: FooCallback::reportResults() is returning data.");
        ALOGI("CLIENT: Waited for %" PRId64 " milliseconds.",
              nanoseconds_to_milliseconds(reportResultsNs - timeLeftNs));

        EXPECT_TRUE(0 <= timeLeftNs && timeLeftNs <= reportResultsNs);

        // two-way method, was supposed to return right away
        EXPECT_TRUE(invokeResults[0].invoked);
        EXPECT_TRUE(invokeResults[0].timeNs <= invokeResults[0].callerBlockedNs);
        EXPECT_TRUE(invokeResults[0].callerBlockedNs <= FooCallback::TOLERANCE_NS);
        // two-way method, was supposed to block caller for DELAY_NS
        EXPECT_TRUE(invokeResults[1].invoked);
        EXPECT_TRUE(invokeResults[1].timeNs <= invokeResults[1].callerBlockedNs);
        EXPECT_TRUE(invokeResults[1].callerBlockedNs <=
                    FooCallback::DELAY_NS + FooCallback::TOLERANCE_NS);
        // one-way method, do not block caller, but body was supposed to block for DELAY_NS
        EXPECT_TRUE(invokeResults[2].invoked);
        EXPECT_TRUE(invokeResults[2].callerBlockedNs <= FooCallback::ONEWAY_TOLERANCE_NS);
        EXPECT_TRUE(invokeResults[2].timeNs <= FooCallback::DELAY_NS + FooCallback::TOLERANCE_NS);
    });
}



TEST_F(HidlTest, FooUseAnEnumTest) {
    ALOGI("CLIENT call useAnEnum.");
    IFoo::SomeEnum sleepy = foo->useAnEnum(IFoo::SomeEnum::quux);
    ALOGI("CLIENT useAnEnum returned %u", (unsigned)sleepy);
    EXPECT_EQ(sleepy, IFoo::SomeEnum::goober);
}

TEST_F(HidlTest, FooHaveAGooberTest) {
    hidl_vec<IFoo::Goober> gooberVecParam;
    gooberVecParam.resize(2);
    gooberVecParam[0].name = "Hello";
    gooberVecParam[1].name = "World";

    ALOGI("CLIENT call haveAGooberVec.");
    EXPECT_OK(foo->haveAGooberVec(gooberVecParam));
    ALOGI("CLIENT haveAGooberVec returned.");

    ALOGI("CLIENT call haveaGoober.");
    EXPECT_OK(foo->haveAGoober(gooberVecParam[0]));
    ALOGI("CLIENT haveaGoober returned.");

    ALOGI("CLIENT call haveAGooberArray.");
    IFoo::Goober gooberArrayParam[20];
    EXPECT_OK(foo->haveAGooberArray(gooberArrayParam));
    ALOGI("CLIENT haveAGooberArray returned.");
}

TEST_F(HidlTest, FooHaveATypeFromAnotherFileTest) {
    ALOGI("CLIENT call haveATypeFromAnotherFile.");
    Abc abcParam{};
    abcParam.x = "alphabet";
    abcParam.y = 3.14f;
    abcParam.z = new native_handle_t();
    EXPECT_OK(foo->haveATypeFromAnotherFile(abcParam));
    ALOGI("CLIENT haveATypeFromAnotherFile returned.");
    delete abcParam.z;
    abcParam.z = NULL;
}

TEST_F(HidlTest, FooHaveSomeStringsTest) {
    ALOGI("CLIENT call haveSomeStrings.");
    hidl_string stringArrayParam[3];
    stringArrayParam[0] = "What";
    stringArrayParam[1] = "a";
    stringArrayParam[2] = "disaster";
    EXPECT_OK(foo->haveSomeStrings(stringArrayParam));
    ALOGI("CLIENT haveSomeStrings returned.");
}

TEST_F(HidlTest, FooHaveAStringVecTest) {
    ALOGI("CLIENT call haveAStringVec.");
    hidl_vec<hidl_string> stringVecParam;
    stringVecParam.resize(3);
    stringVecParam[0] = "What";
    stringVecParam[1] = "a";
    stringVecParam[2] = "disaster";
    EXPECT_OK(foo->haveAStringVec(stringVecParam));
    ALOGI("CLIENT haveAStringVec returned.");
}

TEST_F(HidlTest, FooTransposeMeTest) {
    float in[3][5];
    float k = 1.0f;
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 5; ++j, ++k) {
            in[i][j] = k;
        }
    }

    ALOGI("CLIENT call transposeMe(%s).",
          FloatArray2DToString(&in[0][0], 3, 5).c_str());

    EXPECT_OK(foo->transposeMe(
                &in[0][0],
                [&](const auto &out) {
                    ALOGI("CLIENT transposeMe returned %s.",
                          FloatArray2DToString(out, 5, 3).c_str());

                    for (size_t i = 0; i < 3; ++i) {
                        for (size_t j = 0; j < 5; ++j) {
                            EXPECT_EQ(out[3 * j + i], in[i][j]);
                        }
                    }
                }));
}

TEST_F(HidlTest, FooCallingDrWhoTest) {
    IFoo::MultiDimensional in;

    size_t k = 0;
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 3; ++j, ++k) {
            in.quuxMatrix[i][j].first = ("First " + std::to_string(k)).c_str();
            in.quuxMatrix[i][j].last = ("Last " + std::to_string(15-k)).c_str();
        }
    }

    ALOGI("CLIENT call callingDrWho(%s).",
          MultiDimensionalToString(in).c_str());

    EXPECT_OK(foo->callingDrWho(
                in,
                [&](const auto &out) {
                    ALOGI("CLIENT callingDrWho returned %s.",
                          MultiDimensionalToString(out).c_str());

                    for (size_t i = 0; i < 5; ++i) {
                        for (size_t j = 0; j < 3; ++j) {
                            EXPECT_STREQ(
                                out.quuxMatrix[i][j].first.c_str(),
                                in.quuxMatrix[4-i][2-j].last.c_str());

                            EXPECT_STREQ(
                                out.quuxMatrix[i][j].last.c_str(),
                                in.quuxMatrix[4-i][2-j].first.c_str());
                        }
                    }
                }));
}

TEST_F(HidlTest, BarThisIsNewTest) {
    // Now the tricky part, get access to the derived interface.
    ALOGI("CLIENT call thisIsNew.");
    EXPECT_OK(bar->thisIsNew());
    ALOGI("CLIENT thisIsNew returned.");
}

int main(int argc, char **argv) {

    ::testing::AddGlobalTestEnvironment(new HidlEnvironment);
    ::testing::InitGoogleTest(&argc, argv);
    int status = RUN_ALL_TESTS();

    ALOGI("Test result = %d", status);
    return status;
}
