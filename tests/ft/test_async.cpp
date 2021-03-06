//
// Created by Darwin Yuan on 2020/8/2.
//

#include <nano-caf/core/msg/message.h>
#include <nano-caf/core/actor/sched_actor.h>
#include <nano-caf/core/actor_system.h>
#include <nano-caf/core/actor/actor.h>
#include <iostream>
#include <nanobench.h>
#include <random>
#include <nano-caf/util/caf_log.h>
#include "../ut/test_msgs.h"

using namespace NANO_CAF_NS;
using namespace std::chrono_literals;

struct future_actor : actor {
   const size_t value = 10;
   size_t final_result = 0;

   auto add(size_t a, size_t b) {
      CAF_DEBUG("calc future 1");
      std::random_device r;
      std::default_random_engine regen{r()};
      std::uniform_int_distribution<size_t> uniform(0, 1000);

      unsigned long result = 0;
      for(unsigned long i = 0; i < 100000; i++) {
         result += (a * b + value) + uniform(regen);
      }

      return result;
   }

   auto on_init() noexcept -> void {
      auto future1 = async(&future_actor::add, this, 5, 3);

      future1.then([]([[maybe_unused]]auto r1) {
            CAF_DEBUG("async future1 done = {}", r1);
         });

      auto future2 = async([this]() {
         CAF_DEBUG("calc future 2");
         size_t result = 0;
          size_t a = 20;
          size_t b = 4;
         std::random_device r;
         std::default_random_engine regen{r()};
         std::uniform_int_distribution<size_t> uniform(0, 1000);

         for(size_t i = 0; i < 100000; i++) {
            result += (a * b + value) + uniform(regen);
         }

         return result;
      });

      future2.then([]([[maybe_unused]] auto r2) {
         CAF_DEBUG("async future2 done = {}", r2);});

      auto future3 = async([this]() {
         CAF_DEBUG("calc future 3");
         size_t result = 0;
          size_t a = 20;
          size_t b = 42;
         std::random_device r;
         std::default_random_engine regen{r()};
         std::uniform_int_distribution<size_t> uniform(0, 1000);

         for(size_t i = 0; i < 100000; i++) {
            result += (a * b + value) + uniform(regen);
         }

         return result;
      });

      future3.then([]([[maybe_unused]] auto r3) {
         CAF_DEBUG("async future3 done = {}", r3);
      });

      auto result4 = with(future1, future2, future3).then(
         [this](auto r1, auto  r2, auto r3) {
            final_result = r1 + r2 + r3;
            CAF_DEBUG("all futures done = {}", final_result);
            exit(exit_reason::normal);
         });
   }

   auto handle_message(message&) noexcept -> task_result override {
      return task_result::resume;
   }
};

void run_on_thread(size_t num_of_threads, char const*) {
   actor_system system{num_of_threads};

   CAF_INFO("started");
//   ankerl::nanobench::Bench().minEpochIterations(109).run(name, [&] {
   auto time1 = std::chrono::steady_clock::now();
     for(size_t i=0; i<1000; i++) {

        auto me = system.spawn<future_actor>();
        me.send<test_message>(1);
        me.wait_for_exit();
        me.release();

     }
   auto time2 = std::chrono::steady_clock::now();
     CAF_INFO("elapsed = {}", (time2 - time1).count());

//   });

   system.shutdown();
   auto time3 = std::chrono::steady_clock::now();
   CAF_INFO("shutdown elapsed = {}", (time3 - time2).count());

   std::cout << "--------------------------------" << std::endl;
   for(size_t i=0; i<num_of_threads; i++) {
      std::cout << "worker[" <<i<<"] = " << system.sched_jobs(i) << " jobs" << std::endl;
   }
   std::cout << "================================" << std::endl;
}

#define __(n) n, "3 tasks on " #n " threads"

int main() {
   //spdlog::set_level(spdlog::level::debug);
   run_on_thread(__(1));
   run_on_thread(__(2));
   run_on_thread(__(3));
   run_on_thread(__(4));
   run_on_thread(__(5));

   return 0;
}