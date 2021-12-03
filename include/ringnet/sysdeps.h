//
// Created by volund on 11/16/21.
//

#ifndef RINGNET_SYSDEPS_H
#define RINGNET_SYSDEPS_H

#include <cstdint>
#include <mutex>
#include <memory>
#include <list>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <iostream>
#include <thread>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "no filesystem support ='("
#endif

#if __has_include(<optional>)
#include <optional>
#define opt_type std::optional
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
#define opt_type std::experimental::optional
#else
#error "no optional support ='("
#endif

#endif //RINGNET_SYSDEPS_H
