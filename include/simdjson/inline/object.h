#ifndef SIMDJSON_INLINE_OBJECT_H
#define SIMDJSON_INLINE_OBJECT_H

#include "simdjson/dom/element.h"
#include "simdjson/dom/object.h"
#include "simdjson/portability.h"
#include <cstring>
#include <string>

namespace simdjson {

//
// simdjson_result<dom::object> inline implementation
//
really_inline simdjson_result<dom::object>::simdjson_result() noexcept
    : internal::simdjson_result_base<dom::object>() {}
really_inline simdjson_result<dom::object>::simdjson_result(dom::object value) noexcept
    : internal::simdjson_result_base<dom::object>(std::forward<dom::object>(value)) {}
really_inline simdjson_result<dom::object>::simdjson_result(error_code error) noexcept
    : internal::simdjson_result_base<dom::object>(error) {}

inline simdjson_result<dom::element> simdjson_result<dom::object>::operator[](const std::string_view &key) const noexcept {
  if (error()) { return error(); }
  return first[key];
}
inline simdjson_result<dom::element> simdjson_result<dom::object>::operator[](const char *key) const noexcept {
  if (error()) { return error(); }
  return first[key];
}
inline simdjson_result<dom::element> simdjson_result<dom::object>::at(const std::string_view &json_pointer) const noexcept {
  if (error()) { return error(); }
  return first.at(json_pointer);
}
inline simdjson_result<dom::element> simdjson_result<dom::object>::at_key(const std::string_view &key) const noexcept {
  if (error()) { return error(); }
  return first.at_key(key);
}
inline simdjson_result<dom::element> simdjson_result<dom::object>::at_key_case_insensitive(const std::string_view &key) const noexcept {
  if (error()) { return error(); }
  return first.at_key_case_insensitive(key);
}

#if SIMDJSON_EXCEPTIONS

inline dom::object::iterator simdjson_result<dom::object>::begin() const noexcept(false) {
  if (error()) { throw simdjson_error(error()); }
  return first.begin();
}
inline dom::object::iterator simdjson_result<dom::object>::end() const noexcept(false) {
  if (error()) { throw simdjson_error(error()); }
  return first.end();
}
inline size_t simdjson_result<dom::object>::size() const noexcept(false) {
  if (error()) { throw simdjson_error(error()); }
  return first.size();
}

#endif // SIMDJSON_EXCEPTIONS

namespace dom {

//
// object inline implementation
//
really_inline object::object() noexcept : tape{} {}
really_inline object::object(const internal::tape_ref &_tape) noexcept : tape{_tape} { }
inline object::iterator object::begin() const noexcept {
  return internal::tape_ref(tape.doc, tape.json_index + 1);
}
inline object::iterator object::end() const noexcept {
  return internal::tape_ref(tape.doc, tape.after_element() - 1);
}
inline size_t object::size() const noexcept {
  return tape.scope_count();
}

inline simdjson_result<element> object::operator[](const std::string_view &key) const noexcept {
  return at_key(key);
}
inline simdjson_result<element> object::operator[](const char *key) const noexcept {
  return at_key(key);
}
inline simdjson_result<element> object::at(const std::string_view &json_pointer) const noexcept {
  size_t slash = json_pointer.find('/');
  std::string_view key = json_pointer.substr(0, slash);

  // Grab the child with the given key
  simdjson_result<element> child;

  // If there is an escape character in the key, unescape it and then get the child.
  size_t escape = key.find('~');
  if (escape != std::string_view::npos) {
    // Unescape the key
    std::string unescaped(key);
    do {
      switch (unescaped[escape+1]) {
        case '0':
          unescaped.replace(escape, 2, "~");
          break;
        case '1':
          unescaped.replace(escape, 2, "/");
          break;
        default:
          return INVALID_JSON_POINTER; // "Unexpected ~ escape character in JSON pointer");
      }
      escape = unescaped.find('~', escape+1);
    } while (escape != std::string::npos);
    child = at_key(unescaped);
  } else {
    child = at_key(key);
  }

  // If there is a /, we have to recurse and look up more of the path
  if (slash != std::string_view::npos) {
    child = child.at(json_pointer.substr(slash+1));
  }

  return child;
}
inline simdjson_result<element> object::at_key(const std::string_view &key) const noexcept {
  iterator end_field = end();
  for (iterator field = begin(); field != end_field; ++field) {
    if (field.key_equals(key)) {
      return field.value();
    }
  }
  return NO_SUCH_FIELD;
}
// In case you wonder why we need this, please see
// https://github.com/simdjson/simdjson/issues/323
// People do seek keys in a case-insensitive manner.
inline simdjson_result<element> object::at_key_case_insensitive(const std::string_view &key) const noexcept {
  iterator end_field = end();
  for (iterator field = begin(); field != end_field; ++field) {
    if (field.key_equals_case_insensitive(key)) {
      return field.value();
    }
  }
  return NO_SUCH_FIELD;
}

//
// object::iterator inline implementation
//
really_inline object::iterator::iterator(const internal::tape_ref &_tape) noexcept : tape{_tape} { }
inline const key_value_pair object::iterator::operator*() const noexcept {
  return key_value_pair(key(), value());
}
inline bool object::iterator::operator!=(const object::iterator& other) const noexcept {
  return tape.json_index != other.tape.json_index;
}
inline bool object::iterator::operator==(const object::iterator& other) const noexcept {
  return tape.json_index == other.tape.json_index;
}
inline object::iterator& object::iterator::operator++() noexcept {
  tape.json_index++;
  tape.json_index = tape.after_element();
  return *this;
}
inline std::string_view object::iterator::key() const noexcept {
  return tape.get_string_view();
}
inline uint32_t object::iterator::key_length() const noexcept {
  return tape.get_string_length();
}
inline const char* object::iterator::key_c_str() const noexcept {
  return reinterpret_cast<const char *>(&tape.doc->string_buf[size_t(tape.tape_value()) + sizeof(uint32_t)]);
}
inline element object::iterator::value() const noexcept {
  return element(internal::tape_ref(tape.doc, tape.json_index + 1));
}

/**
 * Design notes:
 * Instead of constructing a string_view and then comparing it with a
 * user-provided strings, it is probably more performant to have dedicated
 * functions taking as a parameter the string we want to compare against
 * and return true when they are equal. That avoids the creation of a temporary
 * std::string_view. Though it is possible for the compiler to avoid entirely
 * any overhead due to string_view, relying too much on compiler magic is
 * problematic: compiler magic sometimes fail, and then what do you do?
 * Also, enticing users to rely on high-performance function is probably better
 * on the long run.
 */

inline bool object::iterator::key_equals(const std::string_view & o) const noexcept {
  // We use the fact that the key length can be computed quickly
  // without access to the string buffer.
  const uint32_t len = key_length();
  if(o.size() == len) {
    // We avoid construction of a temporary string_view instance.
    return (memcmp(o.data(), key_c_str(), len) == 0);
  }
  return false;
}

inline bool object::iterator::key_equals_case_insensitive(const std::string_view & o) const noexcept {
  // We use the fact that the key length can be computed quickly
  // without access to the string buffer.
  const uint32_t len = key_length();
  if(o.size() == len) {
      // See For case-insensitive string comparisons, avoid char-by-char functions
      // https://lemire.me/blog/2020/04/30/for-case-insensitive-string-comparisons-avoid-char-by-char-functions/
      // Note that it might be worth rolling our own strncasecmp function, with vectorization.
      return (simdjson_strncasecmp(o.data(), key_c_str(), len) == 0);
  }
  return false;
}
//
// key_value_pair inline implementation
//
inline key_value_pair::key_value_pair(const std::string_view &_key, element _value) noexcept :
  key(_key), value(_value) {}

inline std::ostream& operator<<(std::ostream& out, const object &value) {
  return out << minify<object>(value);
}
inline std::ostream& operator<<(std::ostream& out, const key_value_pair &value) {
  return out << minify<key_value_pair>(value);
}

} // namespace dom

template<>
inline std::ostream& minifier<dom::object>::print(std::ostream& out) {
  out << '{';
  auto pair = value.begin();
  auto end = value.end();
  if (pair != end) {
    out << minify<dom::key_value_pair>(*pair);
    for (++pair; pair != end; ++pair) {
      out << "," << minify<dom::key_value_pair>(*pair);
    }
  }
  return out << '}';
}

template<>
inline std::ostream& minifier<dom::key_value_pair>::print(std::ostream& out) {
  return out << '"' << internal::escape_json_string(value.key) << "\":" << value.value;
}

#if SIMDJSON_EXCEPTIONS

template<>
inline std::ostream& minifier<simdjson_result<dom::object>>::print(std::ostream& out) {
  if (value.error()) { throw simdjson_error(value.error()); }
  return out << minify<dom::object>(value.first);
}

inline std::ostream& operator<<(std::ostream& out, const simdjson_result<dom::object> &value) noexcept(false) {
  return out << minify<simdjson_result<dom::object>>(value);
}
#endif // SIMDJSON_EXCEPTIONS

} // namespace simdjson

#endif // SIMDJSON_INLINE_OBJECT_H
