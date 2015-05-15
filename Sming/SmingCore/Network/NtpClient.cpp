#include "NtpClient.h"

NtpClient::NtpClient(NtpTimeResultCallback onTimeReceivedCb)
{
	this->onCompleted = onTimeReceivedCb;
	this->server = NTP_SERVER_DEFAULT;
	autoUpdateTimer.initializeMs(NTP_DEFAULT_AUTO_UPDATE_INTERVAL, Delegate<void()>(&NtpClient::requestTime, this));
}

NtpClient::~NtpClient()
{
}

int NtpClient::resolveServer()
{
	if (this->server != NULL)
	{
		struct ip_addr resolved;
		switch (dns_gethostbyname(this->server.c_str(), &resolved,
				staticDnsResponse, (void*) this))
		{
		case ERR_OK:
			serverAddress = resolved;
			return 1;
			break;
		case ERR_INPROGRESS:
			return 0; // currently finding ip, requestTime() will be called later.
		default:
			debugf("DNS Lookup error occurred.");
			return 0;
		}
	}

	return 0;
}

void NtpClient::requestTime()
{
	// Start listening for incomming packets.
	// may already been started in that case nothing happens.
	this->listen(NTP_LISTEN_PORT);
	
	
	if (serverAddress.isNull())
	{
		if (!resolveServer())
		{
			return;
		}
	}

	uint8_t packet[NTP_PACKET_SIZE];

	// Setup the NTP request packet
	memset(packet, 0, NTP_PACKET_SIZE);

	// These are the only required values for a SNTP request. See page 14:
	// https://tools.ietf.org/html/rfc4330 
	// However size of packet should still be 48 bytes.
	packet[0] = (NTP_VERSION << 3 | 0x03); // LI (0 = no warning), Protocol version (4), Client mode (3)
	packet[1] = 0;     	// Stratum, or type of clock, unspecified.

	NtpClient::sendTo(serverAddress, NTP_PORT, (char*) packet, NTP_PACKET_SIZE);
}

void NtpClient::setNtpServer(String server)
{
	this->server = server;
	// force new DNS lookup
	serverAddress = (uint32_t)0;
}

void NtpClient::setNtpServer(IPAddress serverIp)
{
	this->server = NULL;
	this->serverAddress = serverIp;
}

void NtpClient::setAutoQuery(bool autoQuery)
{
	if (autoQuery)
		autoUpdateTimer.start();
	else
		autoUpdateTimer.stop();
}

void NtpClient::setAutoQueryInterval(int seconds)
{
	// minimum 10 seconds interval.
	if (seconds < 10)
		autoUpdateTimer.setIntervalMs(10000);
	else
		autoUpdateTimer.setIntervalMs(seconds * 1000);
}

void NtpClient::onReceive(pbuf *buf, IPAddress remoteIP, uint16_t remotePort)
{
	// We do some basic check to see if it really is a ntp packet we receive.
	// NTP version should be set to same as we used to send, NTP_VERSION
	// Mode should be set to NTP_MODE_SERVER

	if (onCompleted != NULL)
	{
		uint8_t versionMode = pbuf_get_at(buf, 0);
		uint8_t ver = (versionMode & 0b00111000) >> 3;
		uint8_t mode = (versionMode & 0x07);

		if (mode == NTP_MODE_SERVER && ver == NTP_VERSION)
		{
			//Most likely a correct NTP packet received.

			uint8_t data[4];
			pbuf_copy_partial(buf, data, 4, 40); // Copy only timestamp.

			uint32_t timestamp = (data[0] << 24 | data[1] << 16 | data[2] << 8
					| data[3]);

			// Unix time starts on Jan 1 1970, subtract 70 years:
			uint32_t epoch = timestamp - 0x83AA7E80;

			this->onCompleted(*this, epoch);
		}
	}
}

void NtpClient::staticDnsResponse(const char *name, struct ip_addr *ip, void *arg)
{
	// DNS has been resolved

	NtpClient *self = (NtpClient*) arg;

	if (ip != NULL)
	{
		self->serverAddress = *ip;
		// We do a new request since the last one was never done.
		self->requestTime();
	}
}
