# Iperf command

This repository contains `iperf` command based esp-idf console.

## Documentation

### Supported commands:

- Part of official iperf (2.0): https://iperf.fr/iperf-doc.php#doc
  - note: `iperf3` has not been supported.
- And `iperf --abort`

  ```
  iperf  [-suV] [-c <host>] [-p <port>] [-l <length>] [-i <interval>] [-t <time>] [-b <bandwidth>] [-f <format>] [--abort]
    iperf command to measure network performance, through TCP or UDP connections.
    -c, --client=<host>  run in client mode, connecting to <host>
    -s, --server  run in server mode
      -u, --udp  use UDP rather than TCP
    -V, --ipv6_domain  Set the domain to IPv6 (send packets over IPv6)
    -p, --port=<port>  server port to listen on/connect to
    -l, --len=<length>  Set read/write buffer size
    -i, --interval=<interval>  seconds between periodic bandwidth reports
    -t, --time=<time>  time in seconds to transmit for (default 10 secs)
    -b, --bandwidth=<bandwidth> #[kmgKMG] bandwidth to send at in bits/sec (default in bits/sec)
    -f, --format=<format>  'b' = bits/sec 'k' = Kbits/sec 'm' = Mbits/sec
        --abort  abort running iperf
  ```

* [kmgKMG] Indicates options that support a k,m,g,K,M or G suffix Lowercase format characters are 10^3 based and uppercase are 2^n based (e.g. 1k = 1000, 1K = 1024, 1m = 1,000,000 and 1M = 1,048,576)

**Note: The following formats are supported in this iperf command component:**
```
  'b' = bits/sec
  'k' = Kbits/sec
  'm' = Mbits/sec
```

### Installation

- To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

  ```yaml
  dependencies:
    espressif/iperf_cmd:
      version: "^0.1.3"
  ```
- For more details refer [IDF Component Manager](https://docs.espressif.com/projects/idf-component-manager/en/latest/)
