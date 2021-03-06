#ifndef JAVASCRIPT_ENABLED

#include "lws_client.h"
#include "core/io/ip.h"

Error LWSClient::connect_to_host(String p_host, String p_path, uint16_t p_port, bool p_ssl, PoolVector<String> p_protocols) {

	disconnect_from_host();

	IP_Address addr;

	if (!p_host.is_valid_ip_address()) {
		addr = IP::get_singleton()->resolve_hostname(p_host);
	} else {
		addr = p_host;
	}

	ERR_FAIL_COND_V(!addr.is_valid(), ERR_INVALID_PARAMETER);

	// prepare protocols
	if (p_protocols.size() == 0) // default to binary protocol
		p_protocols.append("binary");
	_make_protocols(p_protocols);

	// init lws client
	PoolVector<struct lws_protocols>::Read pr = protocol_structs.read();
	struct lws_context_creation_info info;
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof i);
	memset(&info, 0, sizeof info);

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = &pr[0];
	info.gid = -1;
	info.uid = -1;
	//info.ws_ping_pong_interval = 5;
	info.user = this;
	context = lws_create_context(&info);

	ERR_FAIL_COND_V(context == NULL, FAILED);

	char abuf[1024];
	char hbuf[1024];
	char pbuf[2048];
	String addr_str = (String)addr;
	strncpy(abuf, addr_str.ascii().get_data(), 1024);
	strncpy(hbuf, p_host.utf8().get_data(), 1024);
	strncpy(pbuf, p_path.utf8().get_data(), 2048);

	i.context = context;
	i.protocol = protocol_string.get_data();
	i.address = abuf;
	i.host = hbuf;
	i.path = pbuf;
	i.port = p_port;
	i.ssl_connection = p_ssl;

	lws_client_connect_via_info(&i);
	return OK;
};

int LWSClient::_handle_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

	LWSPeer::PeerData *peer_data = (LWSPeer::PeerData *)user;

	switch (reason) {

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			peer = Ref<LWSPeer>(memnew(LWSPeer));
			peer->set_wsi(wsi);
			peer_data->peer_id = 0;
			peer_data->in_size = 0;
			peer_data->in_count = 0;
			peer_data->out_count = 0;
			peer_data->rbw.resize(16);
			peer_data->rbr.resize(16);
			peer_data->force_close = false;
			emit_signal("connection_established", lws_get_protocol(wsi)->name);
			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			emit_signal("connection_error");
			destroy_context();
			break;

		case LWS_CALLBACK_CLOSED:
			peer_data->in_count = 0;
			peer_data->out_count = 0;
			peer_data->rbw.resize(0);
			peer_data->rbr.resize(0);
			if (peer.is_valid())
				peer->close();
			peer = Ref<LWSPeer>();
			destroy_context();
			emit_signal("connection_closed");
			return 0; // we can end here

		case LWS_CALLBACK_CLIENT_RECEIVE:
			if (peer.is_valid())
				peer->read_wsi(in, len);
			emit_signal("data_received");
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			if (peer_data->force_close)
				return -1;

			if (peer.is_valid())
				peer->write_wsi();
			break;

		default:
			break;
	}

	return 0;
}

Ref<WebSocketPeer> LWSClient::get_peer() const {

	return peer;
}

bool LWSClient::is_connected_to_host() const {

	return peer.is_valid();
};

bool LWSClient::is_connecting_to_host() const {

	return context != NULL;
};

void LWSClient::disconnect_from_host() {

	if (context == NULL)
		return;

	if (peer.is_valid())
		peer->close();
	peer = Ref<LWSPeer>();

	destroy_context();
};

IP_Address LWSClient::get_connected_host() const {

	return IP_Address();
};

uint16_t LWSClient::get_connected_port() const {

	return 1025;
};

LWSClient::LWSClient() {
	context = NULL;
	free_context = false;
	is_polling = false;
};

LWSClient::~LWSClient() {

	disconnect_from_host();
};

#endif // JAVASCRIPT_ENABLED
