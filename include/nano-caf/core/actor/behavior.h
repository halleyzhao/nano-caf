//
// Created by Darwin Yuan on 2020/7/28.
//

#ifndef NANO_CAF_BEHAVIOR_H
#define NANO_CAF_BEHAVIOR_H

#include <nano-caf/util/type_list.h>
#include <nano-caf/util/callable_trait.h>
#include <nano-caf/core/msg/message_trait.h>
#include <nano-caf/core/msg/message_element.h>
#include <nano-caf/core/actor/task_list.h>
#include <tuple>

NANO_CAF_NS_BEGIN

namespace detail {
   template<typename T>
   constexpr bool is_non_const_lref = !std::is_const_v<T> && std::is_lvalue_reference_v<T>;

   template<typename F>
   using first_arg_t = typename callable_trait<F>::template arg<0>;

   template<typename F>
   constexpr bool is_atom = std::is_base_of_v<__atom_signature, std::decay_t<first_arg_t<F>>>;

   template<typename F>
   constexpr bool is_msg_or_atom = std::is_same_v<const msg_id_t, decltype(from_msg_type_to_id<std::decay_t<first_arg_t<F>>>::msg_id)>;

   ////////////////////////////////////////////////////////////////////////////////////////
   template<typename F, typename = void>
   struct verify_behavior_;

   template<typename T>
   struct is_non_rvalue_ref {
      constexpr static bool value = !std::is_rvalue_reference_v<T>;
   };

   template<typename F>
   struct behavior_pattern_base {
      behavior_pattern_base(F&& f) : f_(std::move(f)) {}

      F f_;
   };

   template<size_t N, typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern;

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<0, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         behavior_pattern_base<F>::f_(ATOM_TYPE{});
         return true;
      }
   };

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<1, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         auto [a1] = *body;
         behavior_pattern_base<F>::f_(ATOM_TYPE{}, a1);
         return true;
      }
   };

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<2, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         auto [a1, a2] = *body;
         behavior_pattern_base<F>::f_(ATOM_TYPE{}, a1, a2);
         return true;
      }
   };

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<3, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         auto [a1, a2, a3] = *body;
         behavior_pattern_base<F>::f_(ATOM_TYPE{}, a1, a2, a3);
         return true;
      }
   };

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<4, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         auto [a1, a2, a3, a4] = *body;
         behavior_pattern_base<F>::f_(ATOM_TYPE{}, a1, a2, a3, a4);
         return true;
      }
   };

   template<typename F, typename MSG_TYPE, typename ATOM_TYPE>
   struct behavior_pattern<5, F, MSG_TYPE, ATOM_TYPE>: behavior_pattern_base<F> {
      using behavior_pattern_base<F>::behavior_pattern_base;

      auto operator()(message_element& msg) {
         auto* body = msg.body<MSG_TYPE>();
         if(body == nullptr) return false;
         auto [a1, a2, a3, a4, a5] = *body;
         behavior_pattern_base<F>::f_(ATOM_TYPE{}, a1, a2, a3, a4, a5);
         return true;
      }
   };

   template<typename F>
   struct verify_behavior_<F, std::enable_if_t<is_msg_or_atom<F> && is_atom<F>>> {
      static_assert(!is_non_const_lref<first_arg_t<F>>, "the atom type cannot be non-const-lvalue-ref");

      using decayed_args = typename callable_trait<F>::decayed_args_type::tail;
      using args_type = typename callable_trait<F>::args_type::tail;
      using atom_type = std::decay_t<first_arg_t<F>>;
      using message_type = typename from_atom_to_msg_type<atom_type>::type;
      using fields_types = typename from_msg_type_to_field<message_type>::type::tail;
      using decayed_field_types = typename fields_types::template transform<std::decay_t>;

      static_assert(std::is_same_v<decayed_field_types, decayed_args>, "parameters & message don't match");
      static_assert(args_type::template pred<is_non_rvalue_ref>, "parameter cannot be rvalue-ref type");

      using type = behavior_pattern<decayed_args::size, F, message_type, atom_type>;
   };

   template<typename F>
   struct verify_behavior_<F, std::enable_if_t<is_msg_or_atom<F> && !is_atom<F>>> {
      static_assert((callable_trait<F>::num_of_args == 1), "only message argument is allowed");
      using message_type = std::decay_t<first_arg_t<F>>;
      static_assert(!std::is_pointer_v<message_type>, "don't use pointer, use reference instead");

      struct type {
         type(F&& f) : f_(std::move(f)) {}

         F f_;

         auto operator()(message_element& msg) -> bool {
            auto* body = msg.body<message_type>();
            if(body == nullptr) return false;
            f_(*body);
            return true;
         }
      };
   };

   template<typename F>
   using verify_behavior = verify_behavior_<F, void>;

   template<typename F>
   using verify_behavior_t = typename verify_behavior<F>::type;

   struct msg_handler {
      virtual auto handle_msg(message_element& msg) -> task_result = 0;
      virtual ~msg_handler() = default;
   };

   template<size_t N, typename T, typename = std::enable_if_t<(std::tuple_size<T>::value > N)>>
   auto call_behavior(T& behaviors, message_element& msg) -> task_result {
      auto consumed = std::get<N>(behaviors)(msg);
      return consumed ? task_result::resume : call_behavior<N+1>(behaviors, msg);
   }

   template<size_t N, typename T, typename ... Args>
   auto call_behavior(T& behaviors, message_element& msg, Args...) -> task_result {
      return task_result::skip;
   }

   template<typename ... Args>
   struct behavior_impl : msg_handler {
      behavior_impl(Args&& ... args) : behaviors_{ std::move(args)...} {}

      std::tuple<Args...> behaviors_;

      auto handle_msg(message_element& msg) -> task_result override {
         return call_behavior<0>(behaviors_, msg);
      }
   };
}

struct behavior {
   template<typename ... Args>
   behavior(Args&&...args) {
      static_assert(((callable_trait<Args>::num_of_args > 0) && ...));
      ptr_.reset(new detail::behavior_impl{detail::verify_behavior_t<Args>{std::move(args)}...});
   }

   std::unique_ptr<detail::msg_handler> ptr_{};
};

NANO_CAF_NS_END

#endif //NANO_CAF_BEHAVIOR_H