//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/host.h>
#include <base/mediarouter/media_route_interface.h>
#include <base/publisher/publisher.h>
#include <modules/http_server/http_server_manager.h>
#include <modules/http_server/interceptors/http_request_interceptors.h>
#include <modules/ice/ice.h>

#include <memory>

#include "modules/rtc_signalling/p2p/rtc_p2p_manager.h"
#include "rtc_ice_candidate.h"
#include "rtc_signalling_observer.h"

class RtcSignallingServer : public ov::EnableSharedFromThis<RtcSignallingServer>
{
public:
	RtcSignallingServer(const cfg::Server &server_config);
	~RtcSignallingServer() override = default;

	bool Start(const ov::SocketAddress *address, const ov::SocketAddress *tls_address);
	bool Stop();

	bool AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer);

	bool Disconnect(const info::VHostAppName &vhost_app_name, const ov::String &stream_name, const std::shared_ptr<const SessionDescription> &peer_sdp);

	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &stream_collections);

	int GetTotalPeerCount() const;
	int GetClientPeerCount() const;

protected:
	struct RtcSignallingInfo
	{
		RtcSignallingInfo(const info::VHostAppName &vhost_app_name,
						  const ov::String &host_name, const ov::String &app_name, const ov::String &stream_name,
						  peer_id_t id, std::shared_ptr<RtcPeerInfo> peer_info,
						  std::shared_ptr<const SessionDescription> offer_sdp, std::shared_ptr<SessionDescription> peer_sdp,
						  std::vector<RtcIceCandidate> local_candidates, std::vector<RtcIceCandidate> remote_candidates)
			: vhost_app_name(vhost_app_name),
			  host_name(host_name),
			  app_name(app_name),
			  stream_name(stream_name),
			  id(id),
			  peer_info(std::move(peer_info)),
			  offer_sdp(std::move(offer_sdp)),
			  peer_sdp(std::move(peer_sdp)),
			  local_candidates(std::move(local_candidates)),
			  remote_candidates(std::move(remote_candidates))
		{
		}

		RtcSignallingInfo(const info::VHostAppName &vhost_app_name,
						  const ov::String &host_name, const ov::String &app_name, const ov::String &stream_name)
			: vhost_app_name(vhost_app_name),
			  host_name(host_name),
			  app_name(app_name),
			  stream_name(stream_name)
		{
		}

		// Ex) #default#app_name (host_name + app_name, airensoft.com + app)
		info::VHostAppName vhost_app_name;

		// Ex) airensoft.com
		ov::String host_name;
		// Ex) app
		ov::String app_name;
		// Ex) stream
		ov::String stream_name;

		// id is generated by signalling_server, and it indicates P2P_INVALID_PEER_ID when there is only WebSocket connection and no request offer
		peer_id_t id = P2P_INVALID_PEER_ID;
		// Whether it's a peer used only as a client
		bool peer_was_client = false;

		std::shared_ptr<RtcPeerInfo> peer_info;

		// Offer SDP (SDP of OME/host peer)
		std::shared_ptr<const SessionDescription> offer_sdp;
		// Peer SDP (SDP of host/client peer)
		std::shared_ptr<const SessionDescription> peer_sdp;

		// candidates of OME/host peer
		std::vector<RtcIceCandidate> local_candidates;

		// candidates of host/client peer
		std::vector<RtcIceCandidate> remote_candidates;
	};

	using SdpCallback = std::function<void(std::shared_ptr<SessionDescription> sdp, std::shared_ptr<ov::Error> error)>;

	std::shared_ptr<WebSocketInterceptor> CreateWebSocketInterceptor();

	std::shared_ptr<ov::Error> DispatchCommand(const std::shared_ptr<WebSocketClient> &ws_client, const ov::String &command, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<const WebSocketFrame> &message);
	std::shared_ptr<ov::Error> DispatchRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchAnswer(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidate(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchOfferP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidateP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchStop(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info);

	const cfg::Server _server_config;

	std::shared_ptr<HttpServer> _http_server;
	std::shared_ptr<HttpsServer> _https_server;

	std::vector<std::shared_ptr<RtcSignallingObserver>> _observers;

	std::map<peer_id_t, std::shared_ptr<RtcSignallingInfo>> _client_list;
	std::shared_mutex _client_list_mutex;

	RtcP2PManager _p2p_manager;
};
