// Copyright 2018-2020 Tomoaki Yoshida <yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <nlohmann/json.hpp>

struct ciless {
  static constexpr char lc(const char c) {
    return (c >= 'A' && c <= 'Z') ? c - ('Z' - 'z') : c;
  }
  bool operator()(const std::string& s1, const std::string& s2) const {
    int i = 0;
    while (i < s1.size() && i < s2.size()) {
      if (lc(s1[i]) < lc(s2[i])) return true;
      if (lc(s1[i]) > lc(s2[i])) return false;
      ++i;
    }
    return s1.size() < s2.size();
  }
};

// case insensitive map
template <typename K, typename V, typename C, typename A>
using cimap = std::map<K, V, ciless, A>;

// case insensitive json type
using Json = nlohmann::basic_json<cimap>;
