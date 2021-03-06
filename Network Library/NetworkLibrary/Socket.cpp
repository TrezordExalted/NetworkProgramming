#include "Socket.h"

namespace Net
{
	Net::Socket::Socket(IPVersion ipversion, SocketHandle handle)
		: ipversion(ipversion), handle(handle)
	{
		assert(ipversion == IPVersion::IPv4 || ipversion == IPVersion::IPv6);
	}

	Result Net::Socket::Create()
	{
		assert(ipversion == IPVersion::IPv4 || ipversion == IPVersion::IPv6);

		if (handle != INVALID_SOCKET)
			return Result::GenericError;

		handle = socket(ipversion == IPVersion::IPv4 ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP);

		if (handle == INVALID_SOCKET)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		if (SetSocketOption(SocketOption::TCP_NoDelay, TRUE) != Result::Success)
			return Result::GenericError;

		return Result::Success;
	}

	Result Net::Socket::Close()
	{
		if (handle == INVALID_SOCKET)
			return Result::GenericError;

		int result = closesocket(handle);

		if (result != 0)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		handle = INVALID_SOCKET;
		return Result::Success;
	}

	Result Socket::Bind(IPEndPoint endpoint)
	{
		assert(ipversion == endpoint.GetIPVersion());

		if (ipversion == IPVersion::IPv4)
		{
			sockaddr_in addr = endpoint.GetSockaddrIPv4();
			int result = bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in));
			if (result != 0)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
		}
		else
		{
			sockaddr_in6 addr = endpoint.GetSockaddrIPv6();
			int result = bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in6));
			if (result != 0)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
		}

		return Result::Success;
	}

	Result Socket::Listen(IPEndPoint endpoint, int backlog)
	{
		if (ipversion == IPVersion::IPv6)
			if (SetSocketOption(SocketOption::IPv6_Only, FALSE) != Result::Success)
				return Result::GenericError;

		if (Bind(endpoint) != Result::Success)
			return Result::GenericError;

		int result = listen(handle, backlog);

		if (result != 0)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		return Result::Success;
	}

	Result Socket::Accept(Socket& outSocket)
	{
		assert(ipversion == IPVersion::IPv4 || ipversion == IPVersion::IPv6);

		if (ipversion == IPVersion::IPv4)
		{
			sockaddr_in addr = {};
			int len = sizeof(sockaddr_in);

			SocketHandle acceptedConnectionHandle = accept(handle, reinterpret_cast<sockaddr*>(&addr), &len);
			if (acceptedConnectionHandle == INVALID_SOCKET)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
			IPEndPoint newConnectionEndPoint(reinterpret_cast<sockaddr*>(&addr));
			std::cout << "New connection accepted!" << std::endl;
			newConnectionEndPoint.Print();

			outSocket = Socket(IPVersion::IPv4, acceptedConnectionHandle);
		}
		else
		{
			sockaddr_in6 addr = {};
			int len = sizeof(sockaddr_in6);

			SocketHandle acceptedConnectionHandle = accept(handle, reinterpret_cast<sockaddr*>(&addr), &len);
			if (acceptedConnectionHandle == INVALID_SOCKET)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
			IPEndPoint newConnectionEndPoint(reinterpret_cast<sockaddr*>(&addr));
			std::cout << "New connection accepted!" << std::endl;
			newConnectionEndPoint.Print();

			outSocket = Socket(IPVersion::IPv6, acceptedConnectionHandle);
		}

		return Result::Success;
	}

	Result Socket::Connect(IPEndPoint endpoint)
	{
		assert(ipversion == endpoint.GetIPVersion());

		int result = 0;
		if (ipversion == IPVersion::IPv4)
		{
			sockaddr_in addr = endpoint.GetSockaddrIPv4();
			result = connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in));
			if (result != 0)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
		}
		else
		{
			sockaddr_in6 addr = endpoint.GetSockaddrIPv6();
			result = connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in6));
			if (result != 0)
			{
				int error = WSAGetLastError();
				return Result::GenericError;
			}
		}

		return Result::Success;
	}

	Result Socket::Send(const void* data, int numberOfBytes, int& bytesSent)
	{
		bytesSent = send(handle, reinterpret_cast<const char*>(data), numberOfBytes, NULL);

		if (bytesSent == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		return Result::Success;
	}

	Result Socket::Recv(void* destination, int numberOfBytes, int& bytesReceived)
	{
		bytesReceived = recv(handle, reinterpret_cast<char*>(destination), numberOfBytes, NULL);

		if (bytesReceived == 0)
			return Result::GenericError;

		if (bytesReceived == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		return Result::Success;
	}

	Result Socket::SendAll(const void* data, int numberOfBytes)
	{
		int totalBytesSent = 0;

		while (totalBytesSent < numberOfBytes)
		{
			int bytesRemaining = numberOfBytes - totalBytesSent;
			int bytesSent = 0;
			const char* bufferOffset = reinterpret_cast<const char*>(data) + totalBytesSent;
			Result result = Send(bufferOffset, bytesRemaining, bytesSent);

			if (result != Result::Success)
				return Result::GenericError;

			totalBytesSent += bytesSent;
		}
		return Result::Success;
	}

	Result Socket::RecvAll(void* destination, int numberOfBytes)
	{
		int totalBytesReceived = 0;

		while (totalBytesReceived < numberOfBytes)
		{
			int bytesRemaining = numberOfBytes - totalBytesReceived;
			int bytesReceived = 0;
			char* bufferOffset = reinterpret_cast<char*>(destination) + totalBytesReceived;
			Result result = Recv(bufferOffset, bytesRemaining, bytesReceived);

			if (result != Result::Success)
				return Result::GenericError;

			totalBytesReceived += bytesReceived;
		}
		return Result::Success;
	}

	Result Socket::Send(Packet& packet)
	{
		uint16_t encodedPacketSize = htons(packet.buffer.size());
		Result result = SendAll(&encodedPacketSize, sizeof(uint16_t));
		if (result != Result::Success)
			return Result::GenericError;

		result = SendAll(packet.buffer.data(), packet.buffer.size());
		if (result != Result::Success)
			return Result::GenericError;


		return Result::Success;
	}

	Result Socket::Recv(Packet& packet)
	{
		packet.Clear();

		uint16_t encodedSize = 0;
		Result result = RecvAll(&encodedSize, sizeof(uint16_t));
		if (result != Result::Success)
			return Result::GenericError;

		uint16_t bufferSize = ntohs(encodedSize);
		
		if (bufferSize > maxPacketSize)
			return Result::GenericError;

		packet.buffer.resize(bufferSize);
		result = RecvAll(&packet.buffer[0], bufferSize);
		if (result != Result::Success)
			return Result::GenericError;

		return Result::Success;
	}

	SocketHandle Net::Socket::GetHandle()
	{
		return handle;
	}

	IPVersion Net::Socket::GetIPVersion()
	{
		return ipversion;
	}

	Result Socket::SetSocketOption(SocketOption option, BOOL value)
	{
		int result = 0; 

		switch (option)
		{
		case SocketOption::TCP_NoDelay:
			result = setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&value), sizeof(value));
			break;
		case SocketOption::IPv6_Only:
			result = setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&value), sizeof(value));
			break;
		default:
			return Result::GenericError;
		}

		if (result != 0)
		{
			int error = WSAGetLastError();
			return Result::GenericError;
		}

		return Result::Success;
	}
}