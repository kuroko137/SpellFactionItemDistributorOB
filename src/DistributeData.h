#pragma once

#include "Defs.h"

namespace SpellFactionItemDistributor
{
	inline srell::regex genericRegex{ R"(\((.*?)\))" };

	struct Traits
	{
		Traits() = default;
		explicit Traits(const std::string& a_str);

		// members
		bool          trueRandom{ false };
		std::uint32_t chance{ 100 };
		std::uint32_t amount{ 1 };
		bool remove{ false };
	};

	class DistributeData
	{
	public:
		struct Input
		{
			FormIDOrSet formStr;
			std::string traitsStr;
			std::string record;
			std::string path;
		};

		DistributeData() = default;
		explicit DistributeData(const Input& a_input);

		[[nodiscard]] static std::uint32_t GetFormID(const std::string& a_str);

		// members
		FormIDOrSet formToAdd{};
		Traits    traits{};

		std::string record{};
		std::string path{};
	};

	class DistributeRecordData : public DistributeData
	{
	public:
		DistributeRecordData();
		DistributeRecordData(FormIDOrSet a_id, const Input& a_input, FormIDOrSet a_baseId);

		[[nodiscard]] static FormIDOrSet GetSwapFormID(const std::string& a_str);

		static void GetFormsAll(const std::string& a_path, const std::string& a_str, std::function<void(std::uint32_t, DistributeRecordData&)> a_func);

		static void GetForms(const std::string& a_path, const std::string& a_str, std::function<void(std::uint32_t, DistributeRecordData&)> a_func);
		
		static std::vector<std::string> GetKeywords(std::string keywordString);

		// members
		FormIDOrSet formIDSet{};
		FormIDOrSet formIDSetBase{};
		std::vector<std::string> keywords;
	};

	using SwapDataVec = std::vector<DistributeRecordData>;
	using TransformDataVec = std::vector<DistributeData>;

	using SwapDataConditional = std::unordered_map<FormIDStr, SwapDataVec>;
	using TransformDataConditional = std::unordered_map<FormIDStr, TransformDataVec>;

	using SFIDResult = std::pair<TESObjectREFR*, DistributeRecordData>;
}