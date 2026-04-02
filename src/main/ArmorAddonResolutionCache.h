#pragma once

#include "ArmorVariant.h"

#include <chrono>
#include <functional>
#include <list>
#include <optional>
#include <unordered_map>

class ArmorAddonResolutionCache {
public:
  struct Key {
    RE::FormID ActorFormID{0};
    RE::FormID ArmorAddonFormID{0};

    auto operator==(const Key &) const -> bool = default;
  };

  struct Value {
    const ArmorVariant *ActiveVariant{nullptr};
    std::optional<ArmorVariant::AddonList> ResolvedAddonList;
  };

  struct UpsertResult {
    bool HadEntry{false};
    const ArmorVariant *PreviousActiveVariant{nullptr};
  };

  explicit ArmorAddonResolutionCache(std::size_t a_capacity)
      : _capacity(a_capacity) {}
  ArmorAddonResolutionCache(std::size_t a_capacity,
                            std::chrono::milliseconds a_ttl)
      : _capacity(a_capacity), _ttl(a_ttl) {}

  auto Find(const Key &a_key)
      -> std::optional<std::reference_wrapper<const Value>>;
  auto PeekIgnoringTtl(const Key &a_key) const
      -> std::optional<std::reference_wrapper<const Value>>;
  auto UpsertIgnoringTtl(Key a_key, Value a_value) -> UpsertResult;
  void Insert(Key a_key, Value a_value);
  void Clear();
  void ClearActor(RE::FormID a_actorFormID);

private:
  struct KeyHash {
    auto operator()(const Key &a_key) const noexcept -> std::size_t;
  };

  struct Entry {
    Value Value;
    std::chrono::steady_clock::time_point ExpiresAt;
    std::list<Key>::iterator LruIt;
  };

  void Touch(const Key &a_key,
             std::unordered_map<Key, Entry, KeyHash>::iterator a_it);

  std::size_t _capacity;
  std::chrono::milliseconds _ttl{0};
  std::list<Key> _lru;
  std::unordered_map<Key, Entry, KeyHash> _entries;
};
