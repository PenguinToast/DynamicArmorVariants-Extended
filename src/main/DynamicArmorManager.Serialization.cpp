#include "DynamicArmorManager.h"
#include "Json.h"

#include <mutex>

void DynamicArmorManager::Serialize(SKSE::SerializationInterface *a_skse) {
  std::shared_lock lock(_stateMutex);
  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";

  const auto writer =
      std::unique_ptr<Json::StreamWriter>(builder.newStreamWriter());
  Json::OStringStream ss;
  Json::Value value;

  Json::Value variantOverrides;
  for (auto &[formID, variantMap] : _variantOverrides) {
    Json::Value variantOverride;

    Json::Value variants;
    std::vector<std::pair<std::string, std::uint64_t>> orderedVariants;
    orderedVariants.reserve(variantMap.size());
    for (const auto &[variant, sequence] : variantMap) {
      orderedVariants.emplace_back(variant, sequence);
    }
    std::ranges::sort(orderedVariants, {},
                      &std::pair<std::string, std::uint64_t>::second);

    for (const auto &[variant, _] : orderedVariants) {
      variants.append(variant);
    }

    variantOverride["actor"] = formID;
    variantOverride["variants"] = variants;
    variantOverrides.append(variantOverride);
  }

  value["overrides"] = variantOverrides;

  writer->write(value, std::addressof(ss));
  auto str = ss.str();

  a_skse->WriteRecord(SerializationType, SerializationVersion, str.data(),
                      static_cast<std::uint32_t>(str.size()));
}

void DynamicArmorManager::Deserialize(SKSE::SerializationInterface *a_skse) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _variantOverrides.clear();
  _nextOverrideSequence = 1;
  std::uint32_t type;
  std::uint32_t version;
  std::uint32_t length;
  a_skse->GetNextRecordInfo(type, version, length);

  if (type != SerializationType)
    return;

  if (version != SerializationVersion) {
    logger::info("Cannot deserialize record data from version {}"sv, version);
    return;
  }

  Json::CharReaderBuilder builder;
  const auto reader =
      std::unique_ptr<Json::CharReader>(builder.newCharReader());

  auto buffer = std::make_unique<char[]>(length);
  a_skse->ReadRecordData(buffer.get(), length);

  auto begin = buffer.get();
  auto end = begin + length;

  Json::Value value;
  std::string errs;
  if (!reader->parse(begin, end, std::addressof(value), nullptr)) {
    logger::error("Failed to parse serialized data"sv);
    return;
  }

  if (!value.isObject()) {
    logger::error("Failed to parse serialized data"sv);
    return;
  }

  auto &variantOverrides = value["overrides"];
  if (!variantOverrides.isArray()) {
    logger::error("Failed to parse serialized data"sv);
    return;
  }

  for (auto &variantOverride : variantOverrides) {
    if (!variantOverride.isObject()) {
      continue;
    }

    RE::FormID formID;
    a_skse->ResolveFormID(variantOverride["actor"].asUInt(), formID);

    auto [it, inserted] = _variantOverrides.emplace(
        formID, std::unordered_map<std::string, std::uint64_t>{});
    auto &variantMap = it->second;

    auto &variants = variantOverride["variants"];
    if (!variants.isArray()) {
      continue;
    }

    for (auto &variant : variants) {
      if (variant.isString()) {
        variantMap.insert_or_assign(variant.asString(),
                                    _nextOverrideSequence++);
      }
    }
  }
}

void DynamicArmorManager::Revert() {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _variantOverrides.clear();
  _nextOverrideSequence = 1;
}
