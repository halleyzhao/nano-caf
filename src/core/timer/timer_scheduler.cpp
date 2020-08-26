//
// Created by Darwin Yuan on 2020/8/22.
//

#include <nano-caf/core/timer/timer_scheduler.h>
#include <nano-caf/core/msg/make_message.h>
#include <nano-caf/util/likely.h>
#include <nano-caf/core/actor/actor_handle.h>

NANO_CAF_NS_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::go_sleep() -> void {
   if(timers_.empty()) {
      cv_.wait([this]() {
         return shutdown_.shutdown_notified() || !msg_queue_.blocked();
      });
   } else {
      if(cv_.wait_until(timers_.begin()->first) == std::cv_status::timeout) {
         timer_set::on_timeout(shutdown_);
      }
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////
namespace {
   inline auto cleanup_msgs(message* msgs) {
      while(msgs != nullptr) {
         std::unique_ptr<message> head{msgs};
         msgs = head->next_;
      }
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::handle_msgs(message* msgs) -> void {
   while(msgs != nullptr) {
      std::unique_ptr<message>head{msgs};
      msgs = head->next_;
      head->next_ = nullptr;

      switch(head->msg_type_id_) {
      case start_timer_msg::type_id:
         timer_set::add_timer(std::move(head));
         break;
      case stop_timer_msg::type_id: {
         auto stop_msg = head->body<stop_timer_msg>();
         timer_set::remove_timer(stop_msg->actor, stop_msg->id);
         break;
      }
      case clear_actor_timer_msg::type_id: {
         auto clear_msg = head->body<clear_actor_timer_msg>();
         timer_set::clear_actor_timers(clear_msg->actor);
         break;
      }
      default: break;
      }

      if(__unlikely(shutdown_.shutdown_notified())) {
         cleanup_msgs(msgs);
         return;
      }
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::schedule() -> void {
   while(1) {
      if(shutdown_.shutdown_notified()) break;
      auto msgs = msg_queue_.take_all();
      if(msgs == nullptr) {
         if(msg_queue_.try_block()) {
            go_sleep();
         }
      } else {
         handle_msgs(msgs);
         timer_set::on_timeout(shutdown_);
      }
   }

   msg_queue_.close();
   timer_set::reset();
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::start() -> void {
   thread_ = std::thread([this]{ schedule(); });
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::stop() -> void {
   shutdown_.notify_shutdown();
   cv_.wake_up();
   thread_.join();
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::send_msg(message* msg) -> status_t {
   switch(msg_queue_.enqueue(msg)) {
      case enq_result::ok: return status_t::ok;
      case enq_result::blocked: cv_.wake_up(); return status_t::ok;
      case enq_result::null_msg: return status_t::null_msg;
      case enq_result::closed:   return status_t::msg_queue_closed;
      default: return status_t::failed;
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::start_timer
      ( intrusive_actor_ptr sender
      , timer_spec const& spec
      , bool periodic
      , std::shared_ptr<timeout_callback_t> callback) -> result_t<timer_id_t> {
   if(__unlikely(!sender)) { return status_t::null_sender; }

   timer_id_t id{timer_id_.fetch_add(1, std::memory_order_relaxed)};
   auto status = send_msg(make_message<start_timer_msg>(
      id, std::move(sender), spec, std::chrono::system_clock::now(), periodic, std::move(callback)));
   if(status != status_t::ok) return status;
   return id;
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::stop_timer(const intrusive_actor_ptr& self, timer_id_t id) -> status_t {
   if(__unlikely(!self)) { return status_t::null_sender; }
   return send_msg(make_message<stop_timer_msg>(self.actor_id(), id));
}

/////////////////////////////////////////////////////////////////////////////////////////////
auto timer_scheduler::clear_actor_timer(const intrusive_actor_ptr& self) -> status_t {
   if(__unlikely(!self)) { return status_t::null_sender; }
   return send_msg(make_message<clear_actor_timer_msg>(self.actor_id()));
}

NANO_CAF_NS_END
