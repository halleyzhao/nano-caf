//
// Created by Darwin Yuan on 2020/8/8.
//

#ifndef NANO_CAF_TYPED_ACTOR_HANDLE_H
#define NANO_CAF_TYPED_ACTOR_HANDLE_H

#include <nano-caf/core/actor/requester.h>

NANO_CAF_NS_BEGIN

template<typename ACTOR_INTERFACE>
struct typed_actor_handle : private actor_handle {
   typed_actor_handle(intrusive_actor_ptr ptr = nullptr) : actor_handle{ptr} {}

   using actor_handle::wait_for_exit;
   using actor_handle::release;
   using actor_handle::exit;

   template<typename METHOD_ATOM, typename ... Args,
      typename = std::enable_if_t<requester::is_msg_valid<METHOD_ATOM, ACTOR_INTERFACE, Args...>>>
   auto send(METHOD_ATOM atom, Args&& ... args) {
      return actor_handle::send<typename METHOD_ATOM::msg_type>(std::forward<Args>(args)...);
   }

   template<typename METHOD_ATOM>
   using future_type = std::future<either<result_t<typename METHOD_ATOM::type::result_type>, status_t>>;

public:
   template<typename METHOD_ATOM, typename ... Args,
      typename = std::enable_if_t<requester::is_msg_valid<METHOD_ATOM, ACTOR_INTERFACE, Args...>>>
   auto request(METHOD_ATOM atom, Args&& ... args) {
      auto l = [&, this](auto&& handler) {
         return actor_handle::request<typename METHOD_ATOM::msg_type>(
               std::forward<decltype(handler)>(handler),
               std::forward<Args>(args)...);
      };
      return requester::then_rsp<METHOD_ATOM, decltype(l)>(std::move(l));
   }

   template<typename METHOD_ATOM, typename ... Args,
      typename = std::enable_if_t<requester::is_msg_valid<METHOD_ATOM, ACTOR_INTERFACE, Args...>>>
   auto request(intrusive_actor_ptr from, METHOD_ATOM atom, Args&& ... args)
      -> either<future_type<METHOD_ATOM>, status_t> {
      using result_type = result_t<typename METHOD_ATOM::type::result_type>;
      requester::inter_actor_promise_handler<result_type> promise{ from };
      auto future = promise.promise_.get_future();
      if(auto result = actor_handle::request<typename METHOD_ATOM::msg_type>(
         from,
         promise,
         std::forward<Args>(args)...); result != status_t::ok) {
         return result;
      }

      return future;
   }
};

NANO_CAF_NS_END

#endif //NANO_CAF_TYPED_ACTOR_HANDLE_H
