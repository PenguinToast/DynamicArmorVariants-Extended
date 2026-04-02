#include "ArmorAddonResolutionCache.h"

auto ArmorAddonResolutionCache::Find(const Key &a_key)
    -> std::optional<std::reference_wrapper<const Value>> {
  if (auto it = _entries.find(a_key); it != _entries.end()) {
    if (_ttl.count() > 0 &&
        std::chrono::steady_clock::now() >= it->second.ExpiresAt) {
      _lru.erase(it->second.LruIt);
      _entries.erase(it);
      return std::nullopt;
    }

    Touch(a_key, it);
    return std::cref(it->second.Value);
  }

  return std::nullopt;
}

auto ArmorAddonResolutionCache::PeekIgnoringTtl(const Key &a_key) const
    -> std::optional<std::reference_wrapper<const Value>> {
  if (const auto it = _entries.find(a_key); it != _entries.end()) {
    return std::cref(it->second.Value);
  }

  return std::nullopt;
}

auto ArmorAddonResolutionCache::UpsertIgnoringTtl(Key a_key, Value a_value)
    -> UpsertResult {
  if (const auto it = _entries.find(a_key); it != _entries.end()) {
    const auto previousActiveVariant = it->second.Value.ActiveVariant;
    it->second.Value = std::move(a_value);
    it->second.ExpiresAt = std::chrono::steady_clock::now() + _ttl;
    Touch(a_key, it);
    return UpsertResult{
        .HadEntry = true, .PreviousActiveVariant = previousActiveVariant};
  }

  _lru.push_front(a_key);
  _entries.emplace(a_key,
                   Entry{.Value = std::move(a_value),
                         .ExpiresAt = std::chrono::steady_clock::now() + _ttl,
                         .LruIt = _lru.begin()});

  if (_entries.size() > _capacity) {
    const auto &lruKey = _lru.back();
    _entries.erase(lruKey);
    _lru.pop_back();
  }

  return UpsertResult{};
}

void ArmorAddonResolutionCache::Insert(Key a_key, Value a_value) {
  UpsertIgnoringTtl(std::move(a_key), std::move(a_value));
}

void ArmorAddonResolutionCache::Clear() {
  _entries.clear();
  _lru.clear();
}

void ArmorAddonResolutionCache::ClearActor(const RE::FormID a_actorFormID) {
  for (auto it = _entries.begin(); it != _entries.end();) {
    if (it->first.ActorFormID != a_actorFormID) {
      ++it;
      continue;
    }

    _lru.erase(it->second.LruIt);
    it = _entries.erase(it);
  }
}

auto ArmorAddonResolutionCache::KeyHash::operator()(
    const Key &a_key) const noexcept -> std::size_t {
  const auto actorHash = std::hash<RE::FormID>{}(a_key.ActorFormID);
  const auto addonHash = std::hash<RE::FormID>{}(a_key.ArmorAddonFormID);
  return actorHash ^
         (addonHash + 0x9e3779b9 + (actorHash << 6) + (actorHash >> 2));
}

void ArmorAddonResolutionCache::Touch(
    const Key &a_key, std::unordered_map<Key, Entry, KeyHash>::iterator a_it) {
  _lru.erase(a_it->second.LruIt);
  _lru.push_front(a_key);
  a_it->second.LruIt = _lru.begin();
}
