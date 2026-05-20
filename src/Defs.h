#pragma once

#include <variant>
#include <string>
#include <ranges>
#include <unordered_set>
#include <unordered_map>

#include "obse\PluginAPI.h"
#include "obse\GameObjects.h"
#include "obse\GameData.h"
#include "obse\GameExtraData.h"
#include "obse_common\SafeWrite.h"
#include "obse\Utilities.h"

#include <common/IDebugLog.h>

#include "lib/string.hpp"
#include "lib/rng.hpp"
#include "lib/distribution.hpp"
#include "lib/numeric.hpp"
#include "lib/simpleINI.hpp"

#include "lib/srell.hpp"

namespace string = clib_util::string;
namespace dist = clib_util::distribution;
namespace numeric = clib_util::numeric;

using SeedRNG = clib_util::RNG;

template <class T>
using MinMax = std::pair<T, T>;
template <class T>
using RelData = std::pair<bool, MinMax<T>>;  //relative vs absolute

using FormIDStr = std::variant<UInt32, std::string>;

using FormIDSet = std::unordered_set<UInt32>;
using FormIDOrSet = std::variant<UInt32, FormIDSet>;
using RefOrSet = std::variant<TESObjectREFR*, FormIDSet>;

template <class T>
using FormMap = std::unordered_map<UInt32, T>;
