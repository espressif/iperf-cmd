# Changelog

## [v1.0.1](https://github.com/espressif/iperf-cmd/releases/tag/v1.0.1)

### Bug Fixes

- update function prototypes for -Wstrict-prototypes ([f9083cc8](https://github.com/espressif/iperf-cmd/commit/f9083cc8))

## [v1.0.0](https://github.com/espressif/iperf-cmd/releases/tag/v1.0.0)

### Features

- make iperf task stack size configurable via sdkconfig ([7830069c](https://github.com/espressif/iperf-cmd/commit/7830069c))
- add weak attribute to iperf report output ([b3b47b24](https://github.com/espressif/iperf-cmd/commit/b3b47b24))
- support parallel -P option for iperf client ([b95d21ab](https://github.com/espressif/iperf-cmd/commit/b95d21ab))
- support build linux target with lwip enabled (pass build) ([a5dfbf7f](https://github.com/espressif/iperf-cmd/commit/a5dfbf7f))
- support tos field in iperf ([ff469123](https://github.com/espressif/iperf-cmd/commit/ff469123))
- support iperf instance id "--id" from console ([5dc212d8](https://github.com/espressif/iperf-cmd/commit/5dc212d8))
- (iperf): support setting bandwidth to bps ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- (iperf-cmd): support iperf command to set bandwidth with [kmgKMG] ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- Added support of running multiple instances simultaneously ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Added "bind" option. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Added option to retrieve runtime stats. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

### Bug Fixes

- static iperf logs for check ([135eef1c](https://github.com/espressif/iperf-cmd/commit/135eef1c))
- keep "Socket created" log level info ([e7f43e95](https://github.com/espressif/iperf-cmd/commit/e7f43e95))
- (iperf): add a workaround for the custom structure iperf_report_task_t ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf): rename iperf_report_task_t to iperf_report_task_data_t ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf-cmd): fix a spelling error ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf-cmd): fix ipv6 iperf of iperf command ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- don't print error when socket force closed ([9b144afc](https://github.com/espressif/iperf-cmd/commit/9b144afc))
- Fixed report task timing and so report precision. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Fixed unit conversions. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

### Updates

- update default tcp buffer size for chips without psram ([dfdc7235](https://github.com/espressif/iperf-cmd/commit/dfdc7235))
- change iperf state handler function name and parameter ([af5df607](https://github.com/espressif/iperf-cmd/commit/af5df607))
- changed internal structure of the iperf ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Removed iperf ip union in favor of esp_ip_addr_t. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

### Breaking Changes

- change iperf state handler function name and parameter ([af5df607](https://github.com/espressif/iperf-cmd/commit/af5df607))
- default unit for bandwidth limit value is now changed from Mbps to bps ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- removed iperf_start() and iperf_stop() ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- removed g_iperf_is_running ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- removed iperf_hook_func ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- added iperf_start_instance, iperf_stop_instance ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

## [v0.1.3](https://github.com/espressif/iperf-cmd/releases/tag/v0.1.3)

### Features

- add kconfig for iperf tcp/udp tx/rx length ([c6b69b08](https://github.com/espressif/iperf-cmd/releases/tagc6b69b08))

### Bug Fixes

- fix iperf tcp rx thrpt is 0 when test with Redmi RB06 ([cd59afd3](https://github.com/espressif/iperf-cmd/releases/tagcd59afd3))

### Updates

- change api names, do not use app_xxx ([449fab6a](https://github.com/espressif/iperf-cmd/releases/tag449fab6a))

## [v0.1.2](https://github.com/espressif/iperf-cmd/releases/tag/v0.1.2)

### Bug Fixes

- fix build with ipv6 only ([caea9730](https://github.com/espressif/iperf-cmd/commit/caea9730))
- No debug if non-fattal errorno ENOBUFS received ([55cce9dc](https://github.com/espressif/iperf-cmd/commit/55cce9dc))

## [0.1.1](https://github.com/espressif/iperf-cmd/releases/tag/v0.1.1)

### Features

- Supported `iperf` core engine.
- Supported register iperf hook func.
