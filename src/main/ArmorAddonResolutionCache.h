#pragma once

#include "ArmorVariant.h"

#include <list>
#include <string>
#include <unordered_map>

class ArmorAddonResolutionCache
{
public:
	struct Key
	{
		RE::FormID ActorFormID{ 0 };
		RE::FormID ArmorAddonFormID{ 0 };

		auto operator==(const Key&) const -> bool = default;
	};

	struct Value
	{
		std::string ActiveVariantState;
		const ArmorVariant::AddonList* ResolvedAddonList{ nullptr };
	};

	explicit ArmorAddonResolutionCache(std::size_t a_capacity) :
		_capacity(a_capacity) {}

	auto Find(const Key& a_key) -> const Value*;
	void Insert(Key a_key, Value a_value);
	void Clear();

private:
	struct KeyHash
	{
		auto operator()(const Key& a_key) const noexcept -> std::size_t;
	};

	struct Entry
	{
		Value Value;
		std::list<Key>::iterator LruIt;
	};

	void Touch(
		const Key& a_key,
		std::unordered_map<Key, Entry, KeyHash>::iterator a_it);

	std::size_t _capacity;
	std::list<Key> _lru;
	std::unordered_map<Key, Entry, KeyHash> _entries;
};
