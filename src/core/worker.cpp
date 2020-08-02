//
// Created by Darwin Yuan on 2020/7/24.
//

#include <nano-caf/core/worker.h>
#include <nano-caf/core/resumable.h>
#include <nano-caf/core/actor/actor_control_block.h>
#include <nano-caf/core/coordinator.h>
#include <nano-caf/util/likely.h>

NANO_CAF_NS_BEGIN

//////////////////////////////////////////////////////////////////////
auto worker::take_one() noexcept -> resumable* {
   return job_queue_.pop_front<resumable>();
}

////////////////////////////////////////////////////////////////////
auto worker::external_enqueue(resumable* job) noexcept -> void {
   intrusive_ptr_add_ref(job);
   job_queue_.push_back(job);
   wakeup_worker();
}

////////////////////////////////////////////////////////////////////
auto worker::wait_done() noexcept -> void {
   thread_.join();
   cleanup();
}

////////////////////////////////////////////////////////////////////
auto worker::stop() noexcept -> void {
   struct shutdown : resumable {
      auto resume() noexcept -> result override {
         return result::shutdown_execution_unit;
      }

      auto intrusive_ptr_add_ref_impl() noexcept -> void override {}
      auto intrusive_ptr_release_impl() noexcept -> void override {
         delete this;
      }
   };

   cmd_queue_.push_back(new shutdown{});
   wakeup_worker();
}

////////////////////////////////////////////////////////////////////
using timespan = std::chrono::duration<int64_t, std::nano>;

namespace {
   struct {
      timespan sleep_durations;
      size_t try_times;
      size_t intervals;
   }
   config[3] = {
      {timespan{10'000},      100, 10},
      {timespan{1'000'000},     500, 5},
      {timespan{1000'000'000}, 1, 1}
   };
}

////////////////////////////////////////////////////////////////////
auto worker::goto_bed() noexcept -> void {
   if(__unlikely(strategy_ < 2 && (++tried_times_) >= config[strategy_].try_times)) {
      ++strategy_;
      tried_times_ = 0;
   }

   std::unique_lock<std::mutex> guard(lock_);
   sleeping = true;
   cv_.wait_for(guard, config[strategy_].sleep_durations,
                [&] { return !job_queue_.empty(); });
   sleeping = false;
}

////////////////////////////////////////////////////////////////////
auto worker::wakeup_worker() noexcept -> void {
   std::unique_lock<std::mutex> guard(lock_);
   if (sleeping && (!job_queue_.empty() || !cmd_queue_.empty()))
      cv_.notify_one();
}

////////////////////////////////////////////////////////////////////
auto worker::get_a_job() noexcept -> resumable* {
   if(auto job = cmd_queue_.pop_front<resumable>(); __unlikely(job != nullptr)) return job;
   if(auto job = job_queue_.pop_front<resumable>(); __likely(job != nullptr)) return job;

   if(__unlikely((tried_times_ % config[strategy_].intervals) == 0)) {
      return coordinator_.try_steal(id_);
   }

   return nullptr;
}

////////////////////////////////////////////////////////////////////
auto worker::run() noexcept -> void {
   while (1) {
      auto job = get_a_job();
      if(job != nullptr) {
         sched_jobs_++;
         if(!resume_job(job)) return;
         strategy_ = 0;
         tried_times_ = 0;
      } else {
         goto_bed();
      }
   }
}

////////////////////////////////////////////////////////////////////
auto worker::cleanup() noexcept -> void {
   while (1) {
      auto job = job_queue_.pop_front<resumable>();
      if(job == nullptr) {
         break;
      } else {
         intrusive_ptr_release(job);
      }
   }
}

////////////////////////////////////////////////////////////////////
auto worker::resume_job(resumable* job) noexcept -> bool {
   switch(job->resume()) {
      case resumable::result::resume_later:
         job_queue_.push_back(job);
         break;
      case resumable::result::shutdown_execution_unit:
         intrusive_ptr_release(job);
         return false;
      case resumable::result::done:
      case resumable::result::awaiting_message:
      default:
         intrusive_ptr_release(job);
         break;
   }

   return true;
}

////////////////////////////////////////////////////////////////////
auto worker::launch() noexcept -> void {
   thread_ = std::thread{[this] {
      run();
   }};
}

NANO_CAF_NS_END