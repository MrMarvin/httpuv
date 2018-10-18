#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <Rcpp.h>
#include "thread.h"

// A callback for deleting objects on the main thread using later(). This is
// needed when the object is an Rcpp object or contains one, because deleting
// such objects invoke R's memory management functions.
template <typename T>
void deleter_main(void* obj) {
  ASSERT_MAIN_THREAD()
  // later() passes a void* to the callback, so we have to cast it.
  T* typed_obj = reinterpret_cast<T*>(obj);

  try {
    delete typed_obj;
  } catch (...) {}
}

// Does the same as deleter_main, but checks that it's running on the
// background thread (when thread debugging is enabled).
template <typename T>
void deleter_background(void* obj) {
  ASSERT_BACKGROUND_THREAD()
  T* typed_obj = reinterpret_cast<T*>(obj);

  try {
    delete typed_obj;
  } catch (...) {}
}

// It's not safe to call REprintf from the background thread but we need some
// way to output error messages. R CMD check does not it if the code uses the
// symbols stdout, stderr, and printf, so this function is a way to avoid
// those. It's to calling `fprintf(stderr, ...)`.
inline void err_printf(const char *fmt, ...) {
  const size_t max_size = 4096;
  char buf[max_size];

  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf, max_size, fmt, args);
  va_end(args);

  if (n == -1)
    return;

  ssize_t res = write(STDERR_FILENO, buf, n);
  // This is here simply to avoid a warning about "ignoring return value" of
  // the write(), on some compilers. (Seen with gcc 4.4.7 on RHEL 6)
  res += 0;
}


// For debugging. See Makevars for information on how to enable.
void trace(const std::string& msg);

// Indexing into an empty vector causes assertion failures on some platforms
template <typename T>
T* safe_vec_addr(std::vector<T>& vec) {
  return vec.size() ? &vec[0] : NULL;
}

// Indexing into an empty vector causes assertion failures on some platforms
inline const char* safe_str_addr(const std::string& str) {
  return str.size() ? &str[0] : NULL;
}

inline std::string to_lower(const std::string& str) {
  std::string lowered = str;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), tolower);
  return lowered;
}

template <typename T>
std::string toString(T x) {
  std::stringstream ss;
  ss << x;
  return ss.str();
}

// Given a path, return just the filename part.
inline std::string basename(const std::string &path) {
  // TODO: handle Windows separators
  size_t found_idx = path.find_last_of('/');

  if (found_idx <= 0) {
    return path;
  } else {
    return path.substr(found_idx + 1);
  }
}

// Given a filename, return the extension.
inline std::string find_extension(const std::string &filename) {
  size_t found_idx = filename.find_last_of('.');

  if (found_idx <= 0) {
    return "";
  } else {
    return filename.substr(found_idx + 1);
  }
}

// This is used for converting an Rcpp named vector (T2) to a std::map.
template <typename T, typename T2>
std::map<std::string, T> toMap(T2 x) {
  ASSERT_MAIN_THREAD()

  std::map<std::string, T> strmap;

  if (x.size() == 0) {
    return strmap;
  }

  Rcpp::CharacterVector names = x.names();
  if (names.isNULL()) {
    throw Rcpp::exception("Error converting R object to map<string, T>: vector does not have names.");
  }

  for (int i=0; i<x.size(); i++) {
    std::string name  = Rcpp::as<std::string>(names[i]);
    T           value = Rcpp::as<T>          (x[i]);
    if (name == "") {
      throw Rcpp::exception("Error converting R object to map<string, T>: element has empty name.");
    }

    strmap.insert(
      std::pair<std::string, T>(name, value)
    );
  }

  return strmap;
}


#endif
