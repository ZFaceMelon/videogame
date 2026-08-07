#include "lan_network.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace engine {
namespace {

constexpr std::uint32_t kMagic = 0x53414E44u; // "SAND"
constexpr std::uint16_t kVersion = 1;
constexpr unsigned short kPort = 27015;

#pragma pack(push, 1)
struct WirePacket {
  std::uint32_t magic = kMagic;
  std::uint16_t version = kVersion;
  std::uint16_t type = 0; // 1 client state, 2 host snapshot
  std::uint32_t sequence = 0;
  NetworkPlayerState player{};
  float host_health = 100.0f;
  float client_health = 100.0f;
  std::uint8_t host_alive = 1;
  std::uint8_t client_alive = 1;
};
#pragma pack(pop)

} // namespace

bool LanNetwork::init_from_environment() {
  const char *mode_text = std::getenv("VIDEOGAME_NET_MODE");
  if (!mode_text || !*mode_text) {
    return true;
  }
#ifndef _WIN32
  std::fprintf(stderr, "LAN multiplayer is currently available on Windows only.\n");
  return false;
#else
  mode_ = std::strcmp(mode_text, "host") == 0 ? Mode::Host :
          std::strcmp(mode_text, "client") == 0 ? Mode::Client : Mode::Offline;
  if (mode_ == Mode::Offline) {
    return true;
  }

  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    std::fprintf(stderr, "WSAStartup failed\n");
    return false;
  }
  SOCKET socket_handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_handle == INVALID_SOCKET) {
    std::fprintf(stderr, "Failed to create LAN socket\n");
    WSACleanup();
    return false;
  }
  u_long nonblocking = 1;
  ioctlsocket(socket_handle, FIONBIO, &nonblocking);
  socket_ = static_cast<std::uintptr_t>(socket_handle);

  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(mode_ == Mode::Host ? kPort : 0);
  if (::bind(socket_handle, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0) {
    std::fprintf(stderr, "Could not bind UDP port %u (Windows error %d)\n",
                 kPort, WSAGetLastError());
    shutdown();
    return false;
  }

  if (mode_ == Mode::Client) {
    const char *host_ip = std::getenv("VIDEOGAME_HOST_IP");
    if (!host_ip || !*host_ip) {
      std::fprintf(stderr, "VIDEOGAME_HOST_IP is missing\n");
      shutdown();
      return false;
    }
    sockaddr_in peer{};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(kPort);
    if (inet_pton(AF_INET, host_ip, &peer.sin_addr) != 1) {
      std::fprintf(stderr, "Invalid host IP: %s\n", host_ip);
      shutdown();
      return false;
    }
    std::memcpy(peer_address_, &peer, sizeof(peer));
    peer_address_size_ = sizeof(peer);
    std::fprintf(stderr, "Joining LAN host %s:%u\n", host_ip, kPort);
  } else {
    std::fprintf(stderr, "Hosting LAN match on UDP port %u\n", kPort);
  }
  return true;
#endif
}

void LanNetwork::tick(const NetworkPlayerState &local, float host_health,
                      float client_health, bool host_alive, bool client_alive) {
#ifdef _WIN32
  if (mode_ == Mode::Offline || socket_ == ~std::uintptr_t{0}) {
    return;
  }
  const SOCKET socket_handle = static_cast<SOCKET>(socket_);

  WirePacket incoming{};
  for (;;) {
    sockaddr_in sender{};
    int sender_size = sizeof(sender);
    const int received = recvfrom(socket_handle, reinterpret_cast<char *>(&incoming),
                                  sizeof(incoming), 0,
                                  reinterpret_cast<sockaddr *>(&sender), &sender_size);
    if (received < 0) {
      const int error = WSAGetLastError();
      if (error != WSAEWOULDBLOCK) {
        std::fprintf(stderr, "LAN receive error %d\n", error);
      }
      break;
    }
    if (received != sizeof(WirePacket) || incoming.magic != kMagic ||
        incoming.version != kVersion) {
      continue;
    }
    if (mode_ == Mode::Host && incoming.type == 1) {
      const bool first_connection = !connected_;
      std::memcpy(peer_address_, &sender, sizeof(sender));
      peer_address_size_ = sizeof(sender);
      connected_ = true;
      has_remote_state_ = true;
      remote_state_ = incoming.player;
      if (first_connection) {
        char address[64]{};
        inet_ntop(AF_INET, &sender.sin_addr, address, sizeof(address));
        std::fprintf(stderr, "LAN player joined from %s\n", address);
      }
    } else if (mode_ == Mode::Client && incoming.type == 2) {
      connected_ = true;
      has_remote_state_ = true;
      remote_state_ = incoming.player;
      received_host_health_ = incoming.host_health;
      received_client_health_ = incoming.client_health;
      received_host_alive_ = incoming.host_alive != 0;
      received_client_alive_ = incoming.client_alive != 0;
    }
  }

  WirePacket outgoing{};
  outgoing.type = mode_ == Mode::Host ? 2 : 1;
  outgoing.sequence = ++sequence_;
  outgoing.player = local;
  outgoing.host_health = host_health;
  outgoing.client_health = client_health;
  outgoing.host_alive = host_alive ? 1 : 0;
  outgoing.client_alive = client_alive ? 1 : 0;
  if (peer_address_size_ > 0 && (mode_ == Mode::Client || connected_)) {
    sendto(socket_handle, reinterpret_cast<const char *>(&outgoing), sizeof(outgoing), 0,
           reinterpret_cast<const sockaddr *>(peer_address_), peer_address_size_);
  }
#else
  (void)local;
  (void)host_health;
  (void)client_health;
  (void)host_alive;
  (void)client_alive;
#endif
}

void LanNetwork::shutdown() {
#ifdef _WIN32
  if (socket_ != ~std::uintptr_t{0}) {
    closesocket(static_cast<SOCKET>(socket_));
    socket_ = ~std::uintptr_t{0};
    WSACleanup();
  }
#endif
  mode_ = Mode::Offline;
  connected_ = false;
  has_remote_state_ = false;
}

} // namespace engine
