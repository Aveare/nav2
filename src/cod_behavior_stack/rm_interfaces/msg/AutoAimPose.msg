std_msgs/Header header
uint8 sequence
uint32 mcu_sample_time_ms
float32 yaw
float32 pitch
float32 roll
float32 projectile_speed
uint8 mode
uint8 robot_id

# 0x1022 full gimbal pose; false for legacy 0x1021 frames.
bool full_pose
float32[4] big_yaw_quaternion
float32 big_yaw_rel
float32 small_yaw
float32 barrel_pitch
uint32 big_yaw_sample_time_ms
uint32 small_yaw_sample_time_ms
uint32 pitch_sample_time_ms
uint16 valid_flags
