/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant 
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2016-2020, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

/// \file
///

#include "slikenet/types.h"
#include "slikenet/assert.h"
#include <string.h>
#include <stdio.h>
#include "slikenet/WindowsIncludes.h"
#include "slikenet/WSAStartupSingleton.h"
#include "slikenet/SocketDefines.h"
#include "slikenet/socket2.h"

#if   defined(_WIN32)
// extern __int64 _strtoui64(const char*, char**, int); // needed for Code::Blocks. Does not compile on Visual Studio 2010
// IP_DONTFRAGMENT is different between winsock 1 and winsock 2.  Therefore, Winsock2.h must be linked againt Ws2_32.lib
// winsock.h must be linked against WSock32.lib.  If these two are mixed up the flag won't work correctly
#include "slikenet/WindowsIncludes.h"

#else
#include <sys/socket.h> // used for getnameinfo()
#include <netdb.h>      // used for getnameinfo()
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "slikenet/Itoa.h"
#include "slikenet/SocketLayer.h"
#include "slikenet/SuperFastHash.h"
#include <stdlib.h>
#include "slikenet/linux_adapter.h"
#include "slikenet/osx_adapter.h"

using namespace SLNet;

const char *IPV6_LOOPBACK="::1";
const char *IPV4_LOOPBACK="127.0.0.1";

AddressOrGUID::AddressOrGUID( Packet *packet )
{
	rakNetGuid=packet->guid;
	systemAddress=packet->systemAddress;
}

unsigned long AddressOrGUID::ToInteger( const AddressOrGUID &aog )
{
	if (aog.rakNetGuid!=UNASSIGNED_RAKNET_GUID)
		return RakNetGUID::ToUint32(aog.rakNetGuid);
	return SystemAddress::ToInteger(aog.systemAddress);
}
const char *AddressOrGUID::ToString(bool writePort) const
{
	if (rakNetGuid!=UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString();
	return systemAddress.ToString(writePort);
}
void AddressOrGUID::ToString(bool writePort, char *dest) const
{
	if (rakNetGuid != UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString(dest);
	return systemAddress.ToString(writePort, dest);
}
void AddressOrGUID::ToString(bool writePort, char *dest, size_t destLength) const
{
	if (rakNetGuid!=UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString(dest,destLength);
	return systemAddress.ToString(writePort,dest,destLength);
}

// Return false if IP address. Return true if domain
bool SLNet::NonNumericHostString( const char *host )
{
	size_t i = 0;
	while (host[i] != '\0') {
		// IPV4: natpunch.slikesoft.com
		// IPV6: fe80::7c:31f7:fec4:27de%14
		if ((host[i] >= 'g' && host[i] <= 'z') ||
			(host[i] >= 'G' && host[i] <= 'Z'))
			return true;
		++i;
	}
	return false;
}

SocketDescriptor::SocketDescriptor() {
#ifdef __native_client__
	blockingSocket=false;
#else
	blockingSocket=true;
#endif
	port=0;
	hostAddress[0]=0;
	remotePortRakNetWasStartedOn_PS3_PSP2=0;
	extraSocketOptions=0;
	socketFamily=AF_INET;
}
SocketDescriptor::SocketDescriptor(unsigned short _port, const char *_hostAddress)
{
	#ifdef __native_client__
		blockingSocket=false;
	#else
		blockingSocket=true;
	#endif
	remotePortRakNetWasStartedOn_PS3_PSP2=0;
	port=_port;
	if (_hostAddress)
		strcpy_s(hostAddress, _hostAddress);
	else
		hostAddress[0]=0;
	extraSocketOptions=0;
	socketFamily=AF_INET;
}

// Defaults to not in peer to peer mode for NetworkIDs.  This only sends the localSystemAddress portion in the BitStream class
// This is what you want for client/server, where the server assigns all NetworkIDs and it is unnecessary to transmit the full structure.
// For peer to peer, this will transmit the systemAddress of the system that created the object in addition to localSystemAddress.  This allows
// Any system to create unique ids locally.
// All systems must use the same value for this variable.
//bool RAK_DLL_EXPORT NetworkID::peerToPeerMode=false;

SystemAddress& SystemAddress::operator = ( const SystemAddress& input )
{
	memcpy(&address, &input.address, sizeof(address));
	systemIndex = input.systemIndex;
	debugPort = input.debugPort;
	return *this;
}
bool SystemAddress::EqualsExcludingPort( const SystemAddress& right ) const
{
	return (address.addr4.sin_family==AF_INET && address.addr4.sin_addr.s_addr==right.address.addr4.sin_addr.s_addr)
#if RAKNET_SUPPORT_IPV6==1
		|| (address.addr4.sin_family==AF_INET6 && memcmp(address.addr6.sin6_addr.s6_addr, right.address.addr6.sin6_addr.s6_addr, sizeof(address.addr6.sin6_addr.s6_addr))==0)
#endif
		;
}
unsigned short SystemAddress::GetPort(void) const
{
	return ntohs(address.addr4.sin_port);
}
unsigned short SystemAddress::GetPortNetworkOrder(void) const
{
	return address.addr4.sin_port;
}
void SystemAddress::SetPortHostOrder(unsigned short s)
{
	address.addr4.sin_port=htons(s);
	debugPort=s;
}
void SystemAddress::SetPortNetworkOrder(unsigned short s)
{
	address.addr4.sin_port=s;
	debugPort=ntohs(s);
}
bool SystemAddress::operator==( const SystemAddress& right ) const
{
	return address.addr4.sin_port == right.address.addr4.sin_port && EqualsExcludingPort(right);
}

bool SystemAddress::operator!=( const SystemAddress& right ) const
{
	return (*this==right)==false;
}

bool SystemAddress::operator>( const SystemAddress& right ) const
{
	if (address.addr4.sin_port == right.address.addr4.sin_port)
	{
#if RAKNET_SUPPORT_IPV6==1
		if (address.addr4.sin_family==AF_INET)
			return address.addr4.sin_addr.s_addr>right.address.addr4.sin_addr.s_addr;
		return memcmp(address.addr6.sin6_addr.s6_addr, right.address.addr6.sin6_addr.s6_addr, sizeof(address.addr6.sin6_addr.s6_addr))>0;
#else
		return address.addr4.sin_addr.s_addr>right.address.addr4.sin_addr.s_addr;
#endif
	}
	return address.addr4.sin_port>right.address.addr4.sin_port;
}

bool SystemAddress::operator<( const SystemAddress& right ) const
{
	if (address.addr4.sin_port == right.address.addr4.sin_port)
	{
#if RAKNET_SUPPORT_IPV6==1
		if (address.addr4.sin_family==AF_INET)
			return address.addr4.sin_addr.s_addr<right.address.addr4.sin_addr.s_addr;
		return memcmp(address.addr6.sin6_addr.s6_addr, right.address.addr6.sin6_addr.s6_addr, sizeof(address.addr6.sin6_addr.s6_addr))>0;
#else
		return address.addr4.sin_addr.s_addr<right.address.addr4.sin_addr.s_addr;
#endif
	}
	return address.addr4.sin_port<right.address.addr4.sin_port;
}
int SystemAddress::size(void)
{
#if RAKNET_SUPPORT_IPV6==1
	return sizeof(sockaddr_in6) + sizeof(char);
#else
	return sizeof(uint32_t) + sizeof(unsigned short) + sizeof(char);
#endif
}
unsigned long SystemAddress::ToInteger( const SystemAddress &sa )
{
	unsigned int lastHash = SuperFastHashIncremental ((const char*) & sa.address.addr4.sin_port, sizeof(sa.address.addr4.sin_port), sizeof(sa.address.addr4.sin_port) );
#if RAKNET_SUPPORT_IPV6==1
	if (sa.address.addr4.sin_family==AF_INET)
		return SuperFastHashIncremental ((const char*) & sa.address.addr4.sin_addr.s_addr, sizeof(sa.address.addr4.sin_addr.s_addr), lastHash );
	else
		return SuperFastHashIncremental ((const char*) & sa.address.addr6.sin6_addr.s6_addr, sizeof(sa.address.addr6.sin6_addr.s6_addr), lastHash );
#else
	return SuperFastHashIncremental ((const char*) & sa.address.addr4.sin_addr.s_addr, sizeof(sa.address.addr4.sin_addr.s_addr), lastHash );
#endif
}
unsigned char SystemAddress::GetIPVersion(void) const
{
	if (address.addr4.sin_family==AF_INET)
		return 4;
	return 6;
}
unsigned int SystemAddress::GetIPPROTO(void) const
{
#if RAKNET_SUPPORT_IPV6==1
	if (address.addr4.sin_family==AF_INET)
		return IPPROTO_IP;
	return IPPROTO_IPV6;
#else
	return IPPROTO_IP;
#endif
}
void SystemAddress::SetToLoopback(void)
{
	SetToLoopback(GetIPVersion());
}
void SystemAddress::SetToLoopback(unsigned char ipVersion)
{
	if (ipVersion==4)
	{
		FromString(IPV4_LOOPBACK, 0, ipVersion);
	}
	else
	{
		FromString(IPV6_LOOPBACK, 0, ipVersion);
	}
}
bool SystemAddress::IsLoopback(void) const
{
	if (GetIPVersion()==4)
	{
		// unsigned long l = htonl(address.addr4.sin_addr.s_addr);
		if (htonl(address.addr4.sin_addr.s_addr)==2130706433)
			return true;
		if (address.addr4.sin_addr.s_addr==0)
			return true;
	}
#if RAKNET_SUPPORT_IPV6==1
	else
	{
		const static char localhost[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
		if (memcmp(&address.addr6.sin6_addr, localhost, 16)==0)
			return true;
	}
#endif
	return false;
}
void SystemAddress::ToString_Old(bool writePort, char *dest, char portDelineator) const
{
	if (*this == UNASSIGNED_SYSTEM_ADDRESS)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		strcpy(dest, "UNASSIGNED_SYSTEM_ADDRESS");
#pragma warning(pop)
		return;
	}

	char portStr[2];
	portStr[0] = portDelineator;
	portStr[1] = 0;

	in_addr in;
	in.s_addr = address.addr4.sin_addr.s_addr;
	char buf[1024];
	inet_ntop(AF_INET, &in, buf, 1024);
#pragma warning(push)
#pragma warning(disable:4996)
	strcpy(dest, buf);
#pragma warning(pop)
	if (writePort)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		strcat(dest, portStr);
#pragma warning(pop)
		Itoa(GetPort(), dest + strlen(dest), 10);
	}
}

void SystemAddress::ToString_Old(bool writePort, char *dest, size_t destLength, char portDelineator) const
{
	if (*this==UNASSIGNED_SYSTEM_ADDRESS)
	{
		strcpy_s(dest, destLength, "UNASSIGNED_SYSTEM_ADDRESS");
		return;
	}

	char portStr[2];
	portStr[0]=portDelineator;
	portStr[1]=0;

	in_addr in;
	in.s_addr = address.addr4.sin_addr.s_addr;
	inet_ntop(AF_INET, &in, dest, destLength);
	if (writePort)
	{
		strcat_s(dest, destLength, portStr);
		Itoa(GetPort(), dest+strlen(dest), 10);
	}

}
const char *SystemAddress::ToString(bool writePort, char portDelineator) const
{
	static unsigned char strIndex=0;
#if RAKNET_SUPPORT_IPV6==1
	static const size_t strLength=INET6_ADDRSTRLEN+5+1;
#else
	static const size_t strLength=22+5+1;
#endif
	static char str[8][strLength];

	unsigned char lastStrIndex=strIndex;
	strIndex++;
	ToString(writePort, str[lastStrIndex&7], strLength, portDelineator);
	return (char*) str[lastStrIndex&7];
}
#if RAKNET_SUPPORT_IPV6==1
void SystemAddress::ToString_New(bool writePort, char *dest, char portDelineator) const
{
	int ret;
	(void)ret;

	if (*this == UNASSIGNED_SYSTEM_ADDRESS)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		strcpy(dest, "UNASSIGNED_SYSTEM_ADDRESS");
#pragma warning(pop)
		return;
	}

	if (address.addr4.sin_family == AF_INET)
	{
		ret = getnameinfo((struct sockaddr *) &address.addr4, sizeof(struct sockaddr_in), dest, 22, nullptr, 0, NI_NUMERICHOST);
	}
	else
	{
		ret = getnameinfo((struct sockaddr *) &address.addr6, sizeof(struct sockaddr_in6), dest, INET6_ADDRSTRLEN, nullptr, 0, NI_NUMERICHOST);
	}
	if (ret != 0)
	{
		dest[0] = 0;
	}

	if (writePort)
	{
		unsigned char ch[2];
		ch[0] = portDelineator;
		ch[1] = 0;
#pragma warning(push)
#pragma warning(disable:4996)
		strcat(dest, (const char*)ch);
#pragma warning(pop)
		Itoa(ntohs(address.addr4.sin_port), dest + strlen(dest), 10);
	}
}
void SystemAddress::ToString_New(bool writePort, char *dest, size_t destLength, char portDelineator) const
{
	int ret;
	(void) ret;

	if (*this==UNASSIGNED_SYSTEM_ADDRESS)
	{
		strcpy_s(dest, destLength, "UNASSIGNED_SYSTEM_ADDRESS");
		return;
	}
	
	if (address.addr4.sin_family==AF_INET)
	{
		ret=getnameinfo((struct sockaddr *) &address.addr4, sizeof(struct sockaddr_in), dest, 22, nullptr, 0, NI_NUMERICHOST);
	}
	else
	{
		ret=getnameinfo((struct sockaddr *) &address.addr6, sizeof(struct sockaddr_in6), dest, INET6_ADDRSTRLEN, nullptr, 0, NI_NUMERICHOST);
	}
	if (ret!=0)
	{
		dest[0]=0;
	}

	if (writePort)
	{
		unsigned char ch[2];
		ch[0]=portDelineator;
		ch[1]=0;
		strcat_s(dest, destLength, (const char*) ch);
		Itoa(ntohs(address.addr4.sin_port), dest+strlen(dest), 10);
	}
}
#endif // #if RAKNET_SUPPORT_IPV6!=1
void SystemAddress::ToString(bool writePort, char *dest, char portDelineator) const
{
#if RAKNET_SUPPORT_IPV6!=1
	ToString_Old(writePort, dest, portDelineator);
#else
	ToString_New(writePort, dest, portDelineator);
#endif // #if RAKNET_SUPPORT_IPV6!=1
}
void SystemAddress::ToString(bool writePort, char *dest, size_t destLength, char portDelineator) const
{

#if RAKNET_SUPPORT_IPV6!=1
	ToString_Old(writePort,dest,destLength,portDelineator);
#else
	ToString_New(writePort,dest,destLength,portDelineator);
#endif // #if RAKNET_SUPPORT_IPV6!=1
}
SystemAddress::SystemAddress()
{
	address.addr4.sin_family=AF_INET;
	// used for operator ==
	memset(&address,0,sizeof(address)); address.addr4.sin_family=AF_INET;
	systemIndex=(SystemIndex)-1;
	debugPort=0;
}
SystemAddress::SystemAddress(const SystemAddress& other)
{
	memcpy(&address, &other.address, sizeof(address));
	systemIndex = other.systemIndex;
	debugPort = other.debugPort;
}
SystemAddress::SystemAddress(const char *str)
{
	address.addr4.sin_family=AF_INET;
	SetPortHostOrder(0);
	// #med - reconsider this --- doesn't make sense to allow specifying the port using the default delimiter w/o also allowing specifying the used delimiter
	// i.e. consider changing to FromString(str, 0) or change the ctor to SystemAddress(str, delimiter = '\0') or alternatively to delimiter = '|'
	FromString(str);
	systemIndex=(SystemIndex)-1;
}
SystemAddress::SystemAddress(const char *str, unsigned short port)
{
	address.addr4.sin_family=AF_INET;
	FromStringExplicitPort(str,port);
	systemIndex=(SystemIndex)-1;
}
void SystemAddress::FixForIPVersion(const SystemAddress &boundAddressToSocket)
{
	char str[128];
	ToString(false,str,static_cast<size_t>(128));
	// TODO - what about 255.255.255.255?
	if (strcmp(str, IPV6_LOOPBACK)==0)
	{
		if (boundAddressToSocket.GetIPVersion()==4)
		{
			FromString(IPV4_LOOPBACK,0,4);
		}
	}
	else if (strcmp(str, IPV4_LOOPBACK)==0)
	{
#if RAKNET_SUPPORT_IPV6==1
		if (boundAddressToSocket.GetIPVersion()==6)
		{
			FromString(IPV6_LOOPBACK,0,6);
		}
#endif

// 		if (boundAddressToSocket.GetIPVersion()==4)
// 		{
// 			// Some kind of bug with sendto: returns "The requested address is not valid in its context." if loopback doesn't have the same IP address
// 			address.addr4.sin_addr.s_addr=boundAddressToSocket.address.addr4.sin_addr.s_addr;
// 		}
	}
}
bool SystemAddress::IsLANAddress(void)
{
//	return address.addr4.sin_addr.S_un.S_un_b.s_b1==10 || address.addr4.sin_addr.S_un.s_b1==192;
#if defined(__WIN32__)
	return address.addr4.sin_addr.S_un.S_un_b.s_b1==10 || address.addr4.sin_addr.S_un.S_un_b.s_b1==192;
#else
	return (address.addr4.sin_addr.s_addr >> 24) == 10 || (address.addr4.sin_addr.s_addr >> 24) == 192;
#endif
}
bool SystemAddress::SetBinaryAddress(const char *str, char portDelineator)
{
	size_t delimiterPos = 0;
	size_t stringLength = strlen(str);
	for (; delimiterPos < stringLength; ++delimiterPos) {
		if (str[delimiterPos] == portDelineator) {
			break; // found location of port delimiter
		}
	}

	if (NonNumericHostString(str)) {
		//const char *ip = ( char* ) SocketLayer::DomainNameToIP( str );
		char ip[65];
		ip[0] = '\0';

		// copy the plain hostname (excluding the (optional) port part)
		// #med - change OP_NEW_ARRAY to support size_t type
		char* hostname = OP_NEW_ARRAY<char>(static_cast<int>(delimiterPos + 1), _FILE_AND_LINE_);
		strncpy_s(hostname, delimiterPos + 1, str, delimiterPos);
		RakNetSocket2::DomainNameToIP(hostname, ip);
		OP_DELETE_ARRAY(hostname, _FILE_AND_LINE_);

		if (ip[0] != '\0') {
			inet_pton(AF_INET, ip, &address.addr4.sin_addr.s_addr);
		}
		else {
			*this = UNASSIGNED_SYSTEM_ADDRESS;
			return false;
		}
	}
	else {
		// Split the string into the first part, and the : part
		char IPPart[22];
		// Only write the valid parts, don't change existing if invalid
		//	binaryAddress=UNASSIGNED_SYSTEM_ADDRESS.binaryAddress;
		//	port=UNASSIGNED_SYSTEM_ADDRESS.port;
		size_t index = 0;
		// #med - revise this --- if the hostname length > 22 we'd reject it rather than skipping what is beyond the max length...
		for (; index < delimiterPos && index < 22; ++index) {
			if (str[index] != '.' && (str[index] < '0' || str[index] > '9')) {
				break;
			}
			IPPart[index] = str[index];
		}
		IPPart[index] = '\0';
		if (index > 0) {
			inet_pton(AF_INET, IPPart, &address.addr4.sin_addr.s_addr);
		}
	}

	char portPart[10];
	portPart[0] = '\0';
	if (str[delimiterPos] != '\0') {
		size_t portIndex;
		++delimiterPos; // skip the delimiter
		for (portIndex = 0; portIndex < 10 && str[delimiterPos] != '\0'; ++delimiterPos, ++portIndex) {
			if (str[delimiterPos] < '0' || str[delimiterPos] > '9') {
				break;
			}

			portPart[portIndex] = str[delimiterPos];
		}
		portPart[portIndex] = '\0';
	}
	if (portPart[0] != '\0') {
		// #med - missing / insufficient port range range
		address.addr4.sin_port = htons((unsigned short)atoi(portPart));
		// #med - not set in IPv6 mode
		debugPort = ntohs(address.addr4.sin_port);
	}
	return true;
}

bool SystemAddress::FromString(const char *str, char portDelineator, int ipVersion)
{
#if RAKNET_SUPPORT_IPV6!=1
	(void) ipVersion;
	return SetBinaryAddress(str,portDelineator);
#else
	if (str==0)
	{
		memset(&address,0,sizeof(address));
		address.addr4.sin_family=AF_INET;
		return true;
	}
#if RAKNET_SUPPORT_IPV6==1
	char ipPart[INET6_ADDRSTRLEN];
#else
	char ipPart[INET_ADDRSTRLEN];
#endif
	char portPart[32];

	// TODO - what about 255.255.255.255?
	if (ipVersion==4 && strcmp(str, IPV6_LOOPBACK) == 0) {
		strcpy_s(ipPart,IPV4_LOOPBACK);
	}
	else if (ipVersion==6 && strcmp(str, IPV4_LOOPBACK) == 0) {
		address.addr4.sin_family=AF_INET6;
		strcpy_s(ipPart,IPV6_LOOPBACK);
	}

	int i = 0;
	for (; i < sizeof(ipPart) && str[i] != '\0'; ++i) {
		if (str[i] == portDelineator) {
			// #med - missing error checking, if portPart is non-numeric and/or exceeds max allowed port value
			int j = 0;
			++i; // skip the delimiter
			for (; j < sizeof(portPart) && str[i] != '\0'; ++i, ++j) {
				portPart[j] = str[i];
			}
			portPart[j] = '\0';
			i = i - j - 1; // reset the position to the last position, so the trailing '\0'-terminator is set correctly below
			break;
		}
		ipPart[i] = str[i];
	}
	ipPart[i] = '\0';

	// needed for getaddrinfo
	WSAStartupSingleton::AddRef();

	// This could be a domain, or a printable address such as "192.0.2.1" or "2001:db8:63b3:1::3490"
	// I want to convert it to its binary representation
	addrinfo hints, *servinfo=0;
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_DGRAM;
	if (ipVersion==6)
		hints.ai_family = AF_INET6;
	else if (ipVersion==4)
		hints.ai_family = AF_INET;
	else
		hints.ai_family = AF_UNSPEC;
	INT error = getaddrinfo(ipPart, "", &hints, &servinfo);
	if (servinfo==0 && error != 0)
	{
		if (ipVersion==6)
		{
			ipVersion=4;
			hints.ai_family = AF_UNSPEC;
			getaddrinfo(ipPart, "", &hints, &servinfo);
			if (servinfo==0)
				return false;
		}
		else
			return false;
	}
	RakAssert(servinfo);
	
	unsigned short oldPort = address.addr4.sin_port;
#if RAKNET_SUPPORT_IPV6==1
	if (servinfo->ai_family == AF_INET)
	{
// 		if (ipVersion==6)
// 		{
//			address.addr4.sin_family=AF_INET6;
// 			memset(&address.addr6,0,sizeof(address.addr6));
// 			memcpy(address.addr6.sin6_addr.s6_addr+12,&((struct sockaddr_in *)servinfo->ai_addr)->sin_addr.s_addr,sizeof(unsigned long));
// 		}
// 		else
// 		{
			address.addr4.sin_family=AF_INET;
			memcpy(&address.addr4, (struct sockaddr_in *)servinfo->ai_addr,sizeof(struct sockaddr_in));
//		}
	}
	else
	{
		address.addr4.sin_family=AF_INET6;
		memcpy(&address.addr6, (struct sockaddr_in6 *)servinfo->ai_addr,sizeof(struct sockaddr_in6));
	}
#else
	address.addr4.sin_family=AF_INET4;
	memcpy(&address.addr4, (struct sockaddr_in *)servinfo->ai_addr,sizeof(struct sockaddr_in));
#endif

	freeaddrinfo(servinfo); // free the linked list

	// needed for getaddrinfo
	WSAStartupSingleton::Deref();

	// PORT
	if (portPart[0])
	{
		address.addr4.sin_port=htons((unsigned short) atoi(portPart));
		debugPort=ntohs(address.addr4.sin_port);
	}
	else
	{
		address.addr4.sin_port=oldPort;
	}

	return true;
#endif // #if RAKNET_SUPPORT_IPV6!=1
}
bool SystemAddress::FromStringExplicitPort(const char *str, unsigned short port, int ipVersion)
{
	bool b = FromString(str,(char) 0,ipVersion);
	if (b==false)
	{
		*this=UNASSIGNED_SYSTEM_ADDRESS;
		return false;
	}
	address.addr4.sin_port=htons(port);
	debugPort=ntohs(address.addr4.sin_port);
	return true;
}
void SystemAddress::CopyPort( const SystemAddress& right )
{
	address.addr4.sin_port=right.address.addr4.sin_port;
	debugPort=right.debugPort;
}
RakNetGUID::RakNetGUID()
{
	systemIndex=(SystemIndex)-1;
	*this=UNASSIGNED_RAKNET_GUID;
}
bool RakNetGUID::operator==( const RakNetGUID& right ) const
{
	return g==right.g;
}
bool RakNetGUID::operator!=( const RakNetGUID& right ) const
{
	return g!=right.g;
}
bool RakNetGUID::operator > ( const RakNetGUID& right ) const
{
	return g > right.g;
}
bool RakNetGUID::operator < ( const RakNetGUID& right ) const
{
	return g < right.g;
}
const char *RakNetGUID::ToString(void) const
{
	static unsigned char strIndex=0;
	static char str[8][64];

	unsigned char lastStrIndex=strIndex;
	strIndex++;
	ToString(str[lastStrIndex&7], 64);
	return (char*) str[lastStrIndex&7];
}
void RakNetGUID::ToString(char *dest) const
{
	if (*this == UNASSIGNED_RAKNET_GUID)
#pragma warning(push)
#pragma warning(disable:4996)
		strcpy(dest, "UNASSIGNED_RAKNET_GUID");
#pragma warning(pop)
	else
		//sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
#pragma warning(push)
#pragma warning(disable:4996)
		sprintf(dest, "%" PRINTF_64_BIT_MODIFIER "u", (long long unsigned int) g);
#pragma warning(pop)
		// sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
}
void RakNetGUID::ToString(char *dest, size_t destLength) const
{
	if (*this==UNASSIGNED_RAKNET_GUID)
		strcpy_s(dest, destLength, "UNASSIGNED_RAKNET_GUID");
	else
		//sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
		sprintf_s(dest, destLength, "%" PRINTF_64_BIT_MODIFIER "u", (long long unsigned int) g);
		// sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
}
bool RakNetGUID::FromString(const char *source)
{
	if (source==0)
		return false;

#if   defined(WIN32)
	g=_strtoui64(source, nullptr, 10);
#else
	// Changed from g=strtoull(source,0,10); for android
	g=strtoull(source, nullptr, 10);
#endif
	return true;

}
unsigned long RakNetGUID::ToUint32( const RakNetGUID &g )
{
	return ((unsigned long) (g.g >> 32)) ^ ((unsigned long) (g.g & 0xFFFFFFFF));
}

namespace SLNet
{
	// initialization list
#ifndef SWIG
	const SystemAddress UNASSIGNED_SYSTEM_ADDRESS;
	const RakNetGUID UNASSIGNED_RAKNET_GUID((uint64_t)-1);
#endif
}