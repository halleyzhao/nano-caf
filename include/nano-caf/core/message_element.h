//
// Created by Darwin Yuan on 2020/7/21.
//

#ifndef NANO_CAF_MESSAGE_ELEMENT_H
#define NANO_CAF_MESSAGE_ELEMENT_H

#include <nano-caf/nano-caf-ns.h>

NANO_CAF_NS_BEGIN

struct message_element {
   template<typename T>
   auto body() const -> T& {
      return *reinterpret_cast<T*>(const_cast<message_element*>(this));
   }
public:
   message_element* next;
};

NANO_CAF_NS_END

#endif //NANO_CAF_MESSAGE_ELEMENT_H
