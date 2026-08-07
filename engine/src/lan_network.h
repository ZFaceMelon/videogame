#pragma once

#include <cstdint>

namespace engine {

struct NetworkPlayerState {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  float move_forward = 0.0f;
  float move_right = 0.0f;
  float attack_progress = 0.0f;
  float shoot_progress = 0.0f;
  float ultimate_progress = 0.0f;
  float ultimate_wave_radius = 0.0f;
  float projectile_x = 0.0f;
  float projectile_y = 0.0f;
  float projectile_z = 0.0f;
  float projectile_yaw = 0.0f;
  float projectile_pitch = 0.0f;
  std::uint32_t flags = 0;
  std::uint32_t swing_id = 0;
  std::uint32_t shot_id = 0;
  std::uint32_t ultimate_id = 0;
};

enum NetworkPlayerFlags : std::uint32_t {
  kNetMoving = 1u << 0,
  kNetSprinting = 1u << 1,
  kNetAirborne = 1u << 2,
  kNetFlying = 1u << 3,
  kNetSwinging = 1u << 4,
  kNetShooting = 1u << 5,
  kNetUltimate = 1u << 6,
  kNetProjectile = 1u << 7,
  kNetHidden = 1u << 8,
};

class LanNetwork {
public:
  enum class Mode { Offline, Host, Client };

  bool init_from_environment();
  void tick(const NetworkPlayerState &local, float host_health,
            float client_health, bool host_alive, bool client_alive);
  void shutdown();

  Mode mode() const { return mode_; }
  bool online() const { return mode_ != Mode::Offline; }
  bool connected() const { return connected_; }
  bool has_remote_state() const { return has_remote_state_; }
  const NetworkPlayerState &remote_state() const { return remote_state_; }
  float received_host_health() const { return received_host_health_; }
  float received_client_health() const { return received_client_health_; }
  bool received_host_alive() const { return received_host_alive_; }
  bool received_client_alive() const { return received_client_alive_; }

private:
  Mode mode_ = Mode::Offline;
  std::uintptr_t socket_ = ~std::uintptr_t{0};
  std::uint32_t sequence_ = 0;
  bool connected_ = false;
  bool has_remote_state_ = false;
  NetworkPlayerState remote_state_{};
  float received_host_health_ = 100.0f;
  float received_client_health_ = 100.0f;
  bool received_host_alive_ = true;
  bool received_client_alive_ = true;
  unsigned char peer_address_[32]{};
  int peer_address_size_ = 0;
};

} // namespace engine
