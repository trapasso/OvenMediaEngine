//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/config.h>

#include "audio_map_item.h"
#include "config/items/virtual_hosts/applications/providers/track_map/audio_map.h"
#include "vhost_app_name.h"

// Forward declaration
namespace ocst
{
	class Orchestrator;
	class OrchestratorInternal;
}  // namespace ocst

namespace info
{
	typedef uint32_t application_id_t;
	constexpr application_id_t InvalidApplicationId = std::numeric_limits<application_id_t>::max();
	constexpr application_id_t MinApplicationId = std::numeric_limits<application_id_t>::min();
	constexpr application_id_t MaxApplicationId = (InvalidApplicationId - static_cast<application_id_t>(1));

	class Host;	 // For storing parent
	class Application
	{
	public:
		virtual const char *GetApplicationTypeName()
		{
			return "ApplicationInfo";
		}

		virtual const char *GetPublisherTypeName()
		{
			return "PublisherInfo";
		}

		bool operator==(const Application &app_info) const;
		bool IsValid() const;

		application_id_t GetId() const;
		ov::String GetUUID() const;
		const VHostAppName &GetName() const;
		const Host &GetHostInfo() const;
		bool IsDynamicApp() const;

		template <typename Tpublisher>
		const Tpublisher *GetPublisherCfg() const
		{
			Tpublisher temp_publisher;
			const auto &publishers = _app_config.GetPublishers().GetPublisherList();

			for (auto &publisher_info : publishers)
			{
				if (temp_publisher.GetType() == publisher_info->GetType())
				{
					return dynamic_cast<const Tpublisher *>(publisher_info);
				}
			}

			return nullptr;
		}

		template <typename Tprovider>
		const Tprovider *GetProviderCfg() const
		{
			Tprovider temp_provider;
			const auto &providers = _app_config.GetProviders().GetProviderList();

			for (auto &provider_info : providers)
			{
				if (temp_provider.GetType() == provider_info->GetType())
				{
					return dynamic_cast<const Tprovider *>(provider_info);
				}
			}

			return nullptr;
		}
		const cfg::vhost::app::Application &GetConfig() const;
		cfg::vhost::app::Application &GetConfig();

		// Get number of Audio Map Item
		size_t GetAudioMapItemCount() const;
		// Get Audio Map Item by index
		std::shared_ptr<AudioMapItem> GetAudioMapItem(size_t index) const;
		// Add audio map item from config
		void AddAudioMapItems(const cfg::vhost::app::pvd::AudioMap &audio_map);

	protected:
		// These constructors will be called from Orchestrator
		friend class ocst::Orchestrator;
		friend class ocst::OrchestratorInternal;

		Application(const info::Host &host_info, application_id_t app_id, const VHostAppName &name, cfg::vhost::app::Application app_config, bool is_dynamic_app);
		Application(const info::Host &host_info, application_id_t app_id, const VHostAppName &name, bool is_dynamic_app);

		// This function is created to minimize the creation of temporary instances
		static const Application &GetInvalidApplication();

		application_id_t _application_id = InvalidApplicationId;
		VHostAppName _name;
		ov::String _app_type_name;
		ov::String _publisher_type_name;

		cfg::vhost::app::Application _app_config;

		// This flag determines whether the application was created in config or dynamically generated by PullStream()
		bool _is_dynamic_app = false;

	private:
		std::shared_ptr<Host> _host_info;

		// Add audio map item
		void AddAudioMapItem(const std::shared_ptr<AudioMapItem> &audio_map_item);

		std::vector<std::shared_ptr<AudioMapItem>> _audio_map_items;
	};
}  // namespace info