# Changelog

## [v1.0.0-a2](https://github.com/espressif/iperf-cmd/releases/tag/v1.0.0-a2)

### Features

- support parallel -P option for iperf client ([b95d21ab](https://github.com/espressif/iperf-cmd/commit/b95d21ab))
- support tos field in iperf ([ff469123](https://github.com/espressif/iperf-cmd/commit/ff469123))
- support iperf instance id "--id" from console ([5dc212d8](https://github.com/espressif/iperf-cmd/commit/5dc212d8))
- (iperf): support setting bandwidth to bps ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- (iperf-cmd): support iperf command to set bandwidth with [kmgKMG] ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- Added support of running multiple instances simultaneously ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Added "bind" option. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Added option to retrieve runtime stats. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

### Bug Fixes

- (iperf): add a workaround for the custom structure iperf_report_task_t ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf): rename iperf_report_task_t to iperf_report_task_data_t ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf-cmd): fix a spelling error ([7bc934d4](https://github.com/espressif/iperf-cmd/commit/7bc934d4))
- (iperf-cmd): fix ipv6 iperf of iperf command ([6423c4c2](https://github.com/espressif/iperf-cmd/commit/6423c4c2))
- Fixed report task timing and so report precision. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Fixed unit conversions. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))

### Updates

- change iperf state handler function name and parameter ([af5df607](https://github.com/espressif/iperf-cmd/commit/af5df607))
- changed internal structure of the iperf ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- Removed iperf ip union in favor of esp_ip_addr_t. ([a88fb24d](https://github.com/espressif/iperf-cmd/commit/a88fb24d))
- update dependencies [espressif/iperf](https://components.espressif.com/components/espressif/iperf) to v1.0.0-a2

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

### Updates

- change api names, do not use app_xxx ([449fab6a](https://github.com/espressif/iperf-cmd/releases/tag449fab6a))
- update dependencies [espressif/iperf](https://components.espressif.com/components/espressif/iperf) to v0.1.3

## [v0.1.2](https://github.com/espressif/iperf-cmd/releases/tag/v0.1.2)

### Bug Fixes

- fix build with ipv6 only ([caea9730](https://github.com/espressif/iperf-cmd/commit/caea9730))

## [0.1.1](https://github.com/espressif/iperf-cmd/releases/tag/v0.1.1)

### Features

- Supported `iperf` command.
- Supported specific option `--abort` to stop iperf.
- Supported register iperf hook functions - from [`espressif/iperf`]()
