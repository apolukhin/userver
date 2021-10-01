#include <userver/utest/utest.hpp>

#include <atomic>

#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>

namespace {

struct CountGuard {
  CountGuard(std::atomic<int>& count) : count_(count) { count_++; }
  CountGuard(CountGuard&& other) : count_(other.count_) { count_++; }
  CountGuard(const CountGuard& other) : count_(other.count_) { count_++; }

  ~CountGuard() { count_--; }

 private:
  std::atomic<int>& count_;
};

struct OverloadedFunc final {
  struct Counters final {
    std::size_t ref_func{0};
    std::size_t cref_func{0};
    std::size_t move_func{0};

    std::size_t ref_arg{0};
    std::size_t cref_arg{0};
    std::size_t move_arg{0};
  };

  Counters& counters;

  struct Tag1 final {};
  struct Tag2 final {};
  struct Tag3 final {};

  Tag1 operator()() & { return (++counters.ref_func, Tag1{}); }
  Tag2 operator()() const& { return (++counters.cref_func, Tag2{}); }
  Tag3 operator()() && { return (++counters.move_func, Tag3{}); }

  Tag1 operator()(std::string&) { return (++counters.ref_arg, Tag1{}); }
  Tag2 operator()(const std::string&) { return (++counters.cref_arg, Tag2{}); }
  Tag3 operator()(std::string&&) { return (++counters.move_arg, Tag3{}); }
};

struct CountingConstructions {
  static int constructions;
  static int destructions;

  CountingConstructions() { ++constructions; }
  CountingConstructions(CountingConstructions&&) noexcept { ++constructions; }
  CountingConstructions(const CountingConstructions&) { ++constructions; }

  void operator()() const { /*noop*/
  }

  CountingConstructions& operator=(CountingConstructions&&) = default;
  CountingConstructions& operator=(const CountingConstructions&) = default;

  ~CountingConstructions() { ++destructions; }
};

void ByPtrFunction() {}
void ByRefFunction() {}

int CountingConstructions::constructions = 0;
int CountingConstructions::destructions = 0;

}  // namespace

UTEST(Task, ArgumentsLifetime) {
  std::atomic<int> count = 0;

  EXPECT_EQ(0, count.load());
  auto task = engine::AsyncNoSpan([](CountGuard) {}, CountGuard(count));
  EXPECT_EQ(1, count.load());

  engine::Yield();
  EXPECT_EQ(0, count.load());

  task.Get();
  EXPECT_EQ(0, count.load());
}

UTEST(Task, ArgumentsLifetimeThrow) {
  std::atomic<int> count = 0;

  EXPECT_EQ(0, count.load());
  auto task = engine::AsyncNoSpan(
      [](CountGuard) { throw std::runtime_error("123"); }, CountGuard(count));
  EXPECT_EQ(1, count.load());

  engine::Yield();
  EXPECT_EQ(0, count.load());

  EXPECT_THROW(task.Get(), std::runtime_error);
  EXPECT_EQ(0, count.load());
}

UTEST(Task, FunctionLifetime) {
  std::atomic<int> count = 0;

  EXPECT_EQ(0, count.load());
  auto task = engine::AsyncNoSpan([guard = CountGuard(count)] {});
  EXPECT_EQ(1, count.load());

  engine::Yield();
  EXPECT_EQ(0, count.load());

  task.Get();
  EXPECT_EQ(0, count.load());
}

UTEST(Task, FunctionLifetimeThrow) {
  std::atomic<int> count = 0;

  EXPECT_EQ(0, count.load());
  auto task = engine::AsyncNoSpan(
      [guard = CountGuard(count)] { throw std::runtime_error("123"); });
  EXPECT_EQ(1, count.load());

  engine::Yield();
  EXPECT_EQ(0, count.load());

  EXPECT_THROW(task.Get(), std::runtime_error);
  EXPECT_EQ(0, count.load());
}

UTEST(Async, OverloadSelection) {
  OverloadedFunc::Counters counters{};
  OverloadedFunc tst{counters};
  std::string arg;

  engine::AsyncNoSpan(std::ref(tst)).Wait();
  EXPECT_EQ(counters.ref_func, 1);

  engine::AsyncNoSpan(std::cref(tst)).Wait();
  EXPECT_EQ(counters.cref_func, 1);

  engine::AsyncNoSpan(tst).Wait();
  engine::AsyncNoSpan(std::as_const(tst)).Wait();
  engine::AsyncNoSpan(std::move(tst)).Wait();
  EXPECT_EQ(counters.move_func, 3);

  engine::AsyncNoSpan(tst, std::ref(arg)).Wait();
  EXPECT_EQ(counters.ref_arg, 1);

  engine::AsyncNoSpan(tst, std::cref(arg)).Wait();
  EXPECT_EQ(counters.cref_arg, 1);

  engine::AsyncNoSpan(tst, arg).Wait();
  engine::AsyncNoSpan(tst, std::as_const(arg)).Wait();
  engine::AsyncNoSpan(tst, std::move(arg)).Wait();
  EXPECT_EQ(counters.move_arg, 3);

  EXPECT_NO_THROW(engine::AsyncNoSpan(&ByPtrFunction).Wait());
  EXPECT_NO_THROW(engine::AsyncNoSpan(ByRefFunction).Wait());
}

UTEST(Async, ResourceDeallocation) {
  engine::AsyncNoSpan(CountingConstructions{}).Wait();
  EXPECT_EQ(CountingConstructions::constructions,
            CountingConstructions::destructions);
}

const auto kDeadlineTestsTimeout = std::chrono::milliseconds(100);
const auto kMaxTestTimeout = std::chrono::milliseconds(10000);
const auto kMaxTestDuration = std::chrono::milliseconds(5000);

TEST(Task, CurrentTaskSetDeadline) {
  RunInCoro([] {
    auto start = std::chrono::steady_clock::now();
    auto task = engine::AsyncNoSpan([] {
      engine::current_task::SetDeadline(
          engine::Deadline::FromDuration(kMaxTestTimeout));
      engine::InterruptibleSleepFor(std::chrono::milliseconds(2));
      EXPECT_FALSE(engine::current_task::IsCancelRequested());

      engine::current_task::SetDeadline(
          engine::Deadline::FromDuration(kDeadlineTestsTimeout));
      engine::InterruptibleSleepFor(kMaxTestTimeout);
      EXPECT_TRUE(engine::current_task::IsCancelRequested());
    });

    EXPECT_NO_THROW(task.Get());
    auto finish = std::chrono::steady_clock::now();
    auto duration = finish - start;
    EXPECT_GE(duration, kDeadlineTestsTimeout);
    EXPECT_LT(duration, kMaxTestDuration);
  });
}

TEST(Async, WithDeadline) {
  RunInCoro([] {
    auto start = std::chrono::steady_clock::now();
    std::atomic<bool> started{false};
    auto task = engine::AsyncNoSpan(
        engine::Deadline::FromDuration(kDeadlineTestsTimeout), [&started] {
          started = true;
          EXPECT_FALSE(engine::current_task::IsCancelRequested());
          engine::InterruptibleSleepFor(kMaxTestTimeout);
          EXPECT_TRUE(engine::current_task::IsCancelRequested());
        });

    EXPECT_NO_THROW(task.Get());
    EXPECT_TRUE(started.load());
    auto finish = std::chrono::steady_clock::now();
    auto duration = finish - start;
    EXPECT_GE(duration, kDeadlineTestsTimeout);
    EXPECT_LT(duration, kMaxTestDuration);
  });
}

TEST(Async, WithDeadlineDetach) {
  RunInCoro([] {
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    auto task = engine::AsyncNoSpan([&started, &finished] {
      auto start = std::chrono::steady_clock::now();
      engine::AsyncNoSpan(
          engine::Deadline::FromDuration(kDeadlineTestsTimeout),
          [start, &started, &finished] {
            started = true;
            EXPECT_FALSE(engine::current_task::IsCancelRequested());
            engine::InterruptibleSleepFor(kMaxTestTimeout);
            EXPECT_TRUE(engine::current_task::IsCancelRequested());

            auto finish = std::chrono::steady_clock::now();
            auto duration = finish - start;
            EXPECT_GE(duration, kDeadlineTestsTimeout);
            EXPECT_LT(duration, kMaxTestDuration);
            finished = true;
          })
          .Detach();
    });
    EXPECT_NO_THROW(task.Get());
    EXPECT_TRUE(started.load());
    while (!finished) engine::Yield();
  });
}

UTEST(Async, Critical) {
  auto task = engine::CriticalAsyncNoSpan([] { return true; });
  task.RequestCancel();
  task.WaitFor(std::chrono::milliseconds(100));
  EXPECT_TRUE(task.Get());
}
