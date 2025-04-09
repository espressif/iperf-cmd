# API Reference

## Header files

- [iperf.h](#file-iperfh)

## File iperf.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**iperf\_cfg\_t**](#struct-iperf_cfg_t) <br>_Iperf Configuration._ |
| typedef int8\_t | [**iperf\_id\_t**](#typedef-iperf_id_t)  <br>_iperf instance ID_ |
| enum  | [**iperf\_ip\_type\_t**](#enum-iperf_ip_type_t)  <br>_Iperf IP type._ |
| enum  | [**iperf\_output\_format\_t**](#enum-iperf_output_format_t)  <br>_Iperf output report format._ |
| typedef void(\* | [**iperf\_report\_handler\_func\_t**](#typedef-iperf_report_handler_func_t)  <br>_iperf report handler function to receive runtime details from iperf_ |
| struct | [**iperf\_report\_t**](#struct-iperf_report_t) <br>_Structure of common data present in any iperf report._ |
| enum  | [**iperf\_state\_t**](#enum-iperf_state_t)  <br>_Iperf status._ |
| enum  | [**iperf\_traffic\_type\_t**](#enum-iperf_traffic_type_t)  <br>_Iperf traffic type._ |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**iperf\_get\_report**](#function-iperf_get_report) (iperf\_id\_t id, [**iperf\_report\_t**](#struct-iperf_report_t) \*report) <br>_Get the current performance report of instance._ |
|  esp\_err\_t | [**iperf\_get\_traffic\_type**](#function-iperf_get_traffic_type) (iperf\_id\_t id, iperf\_traffic\_type\_t \*traffic\_type) <br>_Get the traffic type of instance._ |
|  esp\_err\_t | [**iperf\_register\_report\_handler**](#function-iperf_register_report_handler) (iperf\_id\_t id, iperf\_report\_handler\_func\_t handler, void \*priv) <br>_Set report handler during runtime for instance with given id._ |
|  iperf\_id\_t | [**iperf\_start\_instance**](#function-iperf_start_instance) (const [**iperf\_cfg\_t**](#struct-iperf_cfg_t) \*cfg) <br>_Start iperf instance._ |
|  esp\_err\_t | [**iperf\_stop\_instance**](#function-iperf_stop_instance) (iperf\_id\_t id) <br>_Stop iperf instance._ |

## Macros

| Type | Name |
| ---: | :--- |
| define  | [**IPERF\_ALL\_INSTANCES\_ID**](#define-iperf_all_instances_id)  (-1)<br>_Special ID used to perform actions on all running instances._ |
| define  | [**IPERF\_DEFAULT\_CONFIG\_CLIENT**](#define-iperf_default_config_client) (proto, ip) <br>_Default config to run iperf in client mode._ |
| define  | [**IPERF\_DEFAULT\_CONFIG\_SERVER**](#define-iperf_default_config_server) (proto, ip) <br>_Default config to run iperf in server mode._ |
| define  | [**IPERF\_DEFAULT\_INTERVAL**](#define-iperf_default_interval)  3<br> |
| define  | [**IPERF\_DEFAULT\_IPV4\_UDP\_TX\_LEN**](#define-iperf_default_ipv4_udp_tx_len)  CONFIG\_IPERF\_DEF\_IPV4\_UDP\_TX\_BUFFER\_LEN<br> |
| define  | [**IPERF\_DEFAULT\_IPV6\_UDP\_TX\_LEN**](#define-iperf_default_ipv6_udp_tx_len)  CONFIG\_IPERF\_DEF\_IPV6\_UDP\_TX\_BUFFER\_LEN<br> |
| define  | [**IPERF\_DEFAULT\_NO\_BW\_LIMIT**](#define-iperf_default_no_bw_limit)  -1<br> |
| define  | [**IPERF\_DEFAULT\_PORT**](#define-iperf_default_port)  5001<br> |
| define  | [**IPERF\_DEFAULT\_TCP\_RX\_LEN**](#define-iperf_default_tcp_rx_len)  CONFIG\_IPERF\_DEF\_TCP\_RX\_BUFFER\_LEN<br> |
| define  | [**IPERF\_DEFAULT\_TCP\_TX\_LEN**](#define-iperf_default_tcp_tx_len)  CONFIG\_IPERF\_DEF\_TCP\_TX\_BUFFER\_LEN<br> |
| define  | [**IPERF\_DEFAULT\_TIME**](#define-iperf_default_time)  30<br> |
| define  | [**IPERF\_DEFAULT\_TRAFFIC\_TASK\_PRIORITY**](#define-iperf_default_traffic_task_priority)  CONFIG\_IPERF\_DEF\_TRAFFIC\_TASK\_PRIORITY<br> |
| define  | [**IPERF\_DEFAULT\_UDP\_RX\_LEN**](#define-iperf_default_udp_rx_len)  CONFIG\_IPERF\_DEF\_UDP\_RX\_BUFFER\_LEN<br> |
| define  | [**IPERF\_FLAG\_CLIENT**](#define-iperf_flag_client)  BIT(0)<br> |
| define  | [**IPERF\_FLAG\_CLR**](#define-iperf_flag_clr) (cfg, flag) ((cfg) &= (~(flag)))<br> |
| define  | [**IPERF\_FLAG\_REPORT\_NO\_PRINT**](#define-iperf_flag_report_no_print)  BIT(4)<br> |
| define  | [**IPERF\_FLAG\_SERVER**](#define-iperf_flag_server)  BIT(1)<br> |
| define  | [**IPERF\_FLAG\_SET**](#define-iperf_flag_set) (cfg, flag) ((cfg) \|= (flag))<br> |
| define  | [**IPERF\_FLAG\_TCP**](#define-iperf_flag_tcp)  BIT(2)<br> |
| define  | [**IPERF\_FLAG\_UDP**](#define-iperf_flag_udp)  BIT(3)<br> |
| define  | [**IPERF\_IPV4\_ENABLED**](#define-iperf_ipv4_enabled)  LWIP\_IPV4<br> |
| define  | [**IPERF\_IPV6\_ENABLED**](#define-iperf_ipv6_enabled)  LWIP\_IPV6<br> |
| define  | [**IPERF\_REPORT\_TASK\_NAME**](#define-iperf_report_task_name)  "iperf\_report"<br> |
| define  | [**IPERF\_REPORT\_TASK\_PRIORITY**](#define-iperf_report_task_priority)  CONFIG\_IPERF\_DEF\_REPORT\_TASK\_PRIORITY<br> |
| define  | [**IPERF\_REPORT\_TASK\_STACK**](#define-iperf_report_task_stack)  4096<br> |
| define  | [**IPERF\_SOCKET\_ACCEPT\_TIMEOUT**](#define-iperf_socket_accept_timeout)  5<br> |
| define  | [**IPERF\_SOCKET\_RX\_TIMEOUT**](#define-iperf_socket_rx_timeout)  CONFIG\_IPERF\_DEF\_SOCKET\_RX\_TIMEOUT<br> |
| define  | [**IPERF\_SOCKET\_TCP\_TX\_TIMEOUT**](#define-iperf_socket_tcp_tx_timeout)  CONFIG\_IPERF\_DEF\_SOCKET\_TCP\_TX\_TIMEOUT<br> |
| define  | [**IPERF\_TRAFFIC\_TASK\_NAME**](#define-iperf_traffic_task_name)  "iperf\_traffic"<br> |
| define  | [**IPERF\_TRAFFIC\_TASK\_STACK**](#define-iperf_traffic_task_stack)  4096<br> |

## Structures and Types Documentation

### struct `iperf_cfg_t`

_Iperf Configuration._

Variables:

-  int32\_t bw_lim  <br>bandwidth limit in bits/s

-  esp\_ip\_addr\_t destination  <br>destination IP

-  uint16\_t dport  <br>destination port

-  uint32\_t flag  <br>iperf flag

-  iperf\_output\_format\_t format  <br>output format, bits/sec, Kbits/sec, Mbits/sec

-  iperf\_id\_t instance_id  <br>iperf instance id

-  uint32\_t interval  <br>report interval in secs

-  uint16\_t len_send_buf  <br>send buffer length in bytes

-  iperf\_report\_handler\_func\_t report_handler  <br>iperf status report function

-  void \* report_handler_priv  <br>pointer to user's private data later passed to report function

-  esp\_ip\_addr\_t source  <br>source IP

-  uint16\_t sport  <br>source port

-  uint32\_t time  <br>total send time in secs

-  uint8\_t traffic_task_priority  <br>iperf traffic task priority

### typedef `iperf_id_t`

_iperf instance ID_
```c
typedef int8_t iperf_id_t;
```

### enum `iperf_ip_type_t`

_Iperf IP type._
```c
enum iperf_ip_type_t {
    IPERF_IP_TYPE_IPV4,
    IPERF_IP_TYPE_IPV6
};
```

### enum `iperf_output_format_t`

_Iperf output report format._
```c
enum iperf_output_format_t {
    MBITS_PER_SEC,
    KBITS_PER_SEC,
    BITS_PER_SEC
};
```

### typedef `iperf_report_handler_func_t`

_iperf report handler function to receive runtime details from iperf_
```c
typedef void(* iperf_report_handler_func_t) (iperf_id_t id, iperf_state_t iperf_state, void *priv);
```

### struct `iperf_report_t`

_Structure of common data present in any iperf report._

Variables:

-  float average_bandwidth  <br>average bandwidth from start to end

-  double curr_total_transfer  <br>current data transferred since iperf has started

-  uint32\_t current_time_sec  <br>current time since iperf has started

-  iperf\_output\_format\_t output_format  <br>output format, Mbits or Kbits

-  float period_bandwidth  <br>bandwidth during current interval

-  float period_transfer  <br>data transferred during current interval

### enum `iperf_state_t`

_Iperf status._
```c
enum iperf_state_t {
    IPERF_STARTED,
    IPERF_STOPPED,
    IPERF_RUNNING,
    IPERF_CLOSED
};
```

### enum `iperf_traffic_type_t`

_Iperf traffic type._
```c
enum iperf_traffic_type_t {
    IPERF_TCP_SERVER,
    IPERF_TCP_CLIENT,
    IPERF_UDP_SERVER,
    IPERF_UDP_CLIENT
};
```


## Functions Documentation

### function `iperf_get_report`

_Get the current performance report of instance._
```c
esp_err_t iperf_get_report (
    iperf_id_t id,
    iperf_report_t *report
) 
```


**Warning:**

Not thread safe. Recommended to be used only inside `report_handler` callback and only when iperf state is`IPERF_RUNNING` or`IPERF_CLOSED`.



**Parameters:**


* `id` iperf instance ID 
* `report` pointer where to copy the report 


**Returns:**



* ESP\_OK on success
* ESP\_ERR\_INVALID\_ARG if invalid argument
* ESP\_ERR\_INVALID\_STATE if instance with associated ID was not found
### function `iperf_get_traffic_type`

_Get the traffic type of instance._
```c
esp_err_t iperf_get_traffic_type (
    iperf_id_t id,
    iperf_traffic_type_t *traffic_type
) 
```


**Note:**

Not thread safe. Recommended to be used only inside `report_handler` callback.



**Parameters:**


* `id` iperf instance ID 
* `traffic_type` pointer where to store traffic type 


**Returns:**



* ESP\_OK on success
* ESP\_ERR\_INVALID\_ARG if invalid argument
* ESP\_ERR\_INVALID\_STATE if instance with associated ID was not found
### function `iperf_register_report_handler`

_Set report handler during runtime for instance with given id._
```c
esp_err_t iperf_register_report_handler (
    iperf_id_t id,
    iperf_report_handler_func_t handler,
    void *priv
) 
```


**Note:**

This function is not thread safe. Recommended to be used only inside `report_handler` callback.



**Parameters:**


* `id` iperf instance ID 
* `handler` function pointer to the handler 
* `priv` pointer to user's private data later passed to the report function


**Returns:**



* ESP\_OK on success
* ESP\_ERR\_INVALID\_ARG if invalid argument
* ESP\_ERR\_INVALID\_STATE if instance with associated ID was not found
### function `iperf_start_instance`

_Start iperf instance._
```c
iperf_id_t iperf_start_instance (
    const iperf_cfg_t *cfg
) 
```


**Parameters:**


* `cfg` pointer to traffic config structure


**Returns:**



* iperf instance ID
* -1 if couldn't start new instance
### function `iperf_stop_instance`

_Stop iperf instance._
```c
esp_err_t iperf_stop_instance (
    iperf_id_t id
) 
```


**Parameters:**


* `id` iperf instance ID. Setting to `IPERF_ALL_INSTANCES_ID` stops all running instances


**Returns:**



* ESP\_OK on success
* ESP\_FAIL if instance with given id was not found
* ESP\_ERR\_INVALID\_ARG if invalid id is provided (id &lt; 0)

## Macros Documentation

### define `IPERF_ALL_INSTANCES_ID`

_Special ID used to perform actions on all running instances._
```c
#define IPERF_ALL_INSTANCES_ID (-1)
```


This macro can be passed to APIs that consume instance IDs to indicate that the action should apply to all running application instances.



**Note:**

Not all APIs support this special ID. Refer to the specific API documentation to determine whether this feature is supported.
### define `IPERF_DEFAULT_CONFIG_CLIENT`

_Default config to run iperf in client mode._
```c
#define IPERF_DEFAULT_CONFIG_CLIENT (
    proto,
    ip
) {                                           \
                                                    .flag = proto | IPERF_FLAG_CLIENT,      \
                                                    .format = MBITS_PER_SEC,                \
                                                    .interval = IPERF_DEFAULT_INTERVAL,     \
                                                    .time = IPERF_DEFAULT_TIME,             \
                                                    .bw_lim = IPERF_DEFAULT_NO_BW_LIMIT,    \
                                                    .destination = ip,                      \
                                                    .dport = IPERF_DEFAULT_PORT             \
                                                }
```


**Parameters:**


* `proto` protocol flag (TCP/UDP) 
* `ip` `esp_ip_addr_t` value of source ip
### define `IPERF_DEFAULT_CONFIG_SERVER`

_Default config to run iperf in server mode._
```c
#define IPERF_DEFAULT_CONFIG_SERVER (
    proto,
    ip
) {                                           \
                                                    .flag = proto | IPERF_FLAG_SERVER,      \
                                                    .format = MBITS_PER_SEC,                \
                                                    .interval = IPERF_DEFAULT_INTERVAL,     \
                                                    .time = IPERF_DEFAULT_TIME,             \
                                                    .bw_lim = IPERF_DEFAULT_NO_BW_LIMIT,    \
                                                    .source = ip,                           \
                                                    .sport = IPERF_DEFAULT_PORT             \
                                                }
```


**Parameters:**


* `proto` protocol flag (TCP/UDP) 
* `ip` `esp_ip_addr_t` value of destination ip
### define `IPERF_DEFAULT_INTERVAL`

```c
#define IPERF_DEFAULT_INTERVAL 3
```

### define `IPERF_DEFAULT_IPV4_UDP_TX_LEN`

```c
#define IPERF_DEFAULT_IPV4_UDP_TX_LEN CONFIG_IPERF_DEF_IPV4_UDP_TX_BUFFER_LEN
```

### define `IPERF_DEFAULT_IPV6_UDP_TX_LEN`

```c
#define IPERF_DEFAULT_IPV6_UDP_TX_LEN CONFIG_IPERF_DEF_IPV6_UDP_TX_BUFFER_LEN
```

### define `IPERF_DEFAULT_NO_BW_LIMIT`

```c
#define IPERF_DEFAULT_NO_BW_LIMIT -1
```

### define `IPERF_DEFAULT_PORT`

```c
#define IPERF_DEFAULT_PORT 5001
```

### define `IPERF_DEFAULT_TCP_RX_LEN`

```c
#define IPERF_DEFAULT_TCP_RX_LEN CONFIG_IPERF_DEF_TCP_RX_BUFFER_LEN
```

### define `IPERF_DEFAULT_TCP_TX_LEN`

```c
#define IPERF_DEFAULT_TCP_TX_LEN CONFIG_IPERF_DEF_TCP_TX_BUFFER_LEN
```

### define `IPERF_DEFAULT_TIME`

```c
#define IPERF_DEFAULT_TIME 30
```

### define `IPERF_DEFAULT_TRAFFIC_TASK_PRIORITY`

```c
#define IPERF_DEFAULT_TRAFFIC_TASK_PRIORITY CONFIG_IPERF_DEF_TRAFFIC_TASK_PRIORITY
```

### define `IPERF_DEFAULT_UDP_RX_LEN`

```c
#define IPERF_DEFAULT_UDP_RX_LEN CONFIG_IPERF_DEF_UDP_RX_BUFFER_LEN
```

### define `IPERF_FLAG_CLIENT`

```c
#define IPERF_FLAG_CLIENT BIT(0)
```

### define `IPERF_FLAG_CLR`

```c
#define IPERF_FLAG_CLR (
    cfg,
    flag
) ((cfg) &= (~(flag)))
```

### define `IPERF_FLAG_REPORT_NO_PRINT`

```c
#define IPERF_FLAG_REPORT_NO_PRINT BIT(4)
```

### define `IPERF_FLAG_SERVER`

```c
#define IPERF_FLAG_SERVER BIT(1)
```

### define `IPERF_FLAG_SET`

```c
#define IPERF_FLAG_SET (
    cfg,
    flag
) ((cfg) |= (flag))
```

### define `IPERF_FLAG_TCP`

```c
#define IPERF_FLAG_TCP BIT(2)
```

### define `IPERF_FLAG_UDP`

```c
#define IPERF_FLAG_UDP BIT(3)
```

### define `IPERF_IPV4_ENABLED`

```c
#define IPERF_IPV4_ENABLED LWIP_IPV4
```

### define `IPERF_IPV6_ENABLED`

```c
#define IPERF_IPV6_ENABLED LWIP_IPV6
```

### define `IPERF_REPORT_TASK_NAME`

```c
#define IPERF_REPORT_TASK_NAME "iperf_report"
```

### define `IPERF_REPORT_TASK_PRIORITY`

```c
#define IPERF_REPORT_TASK_PRIORITY CONFIG_IPERF_DEF_REPORT_TASK_PRIORITY
```

### define `IPERF_REPORT_TASK_STACK`

```c
#define IPERF_REPORT_TASK_STACK 4096
```

### define `IPERF_SOCKET_ACCEPT_TIMEOUT`

```c
#define IPERF_SOCKET_ACCEPT_TIMEOUT 5
```

### define `IPERF_SOCKET_RX_TIMEOUT`

```c
#define IPERF_SOCKET_RX_TIMEOUT CONFIG_IPERF_DEF_SOCKET_RX_TIMEOUT
```

### define `IPERF_SOCKET_TCP_TX_TIMEOUT`

```c
#define IPERF_SOCKET_TCP_TX_TIMEOUT CONFIG_IPERF_DEF_SOCKET_TCP_TX_TIMEOUT
```

### define `IPERF_TRAFFIC_TASK_NAME`

```c
#define IPERF_TRAFFIC_TASK_NAME "iperf_traffic"
```

### define `IPERF_TRAFFIC_TASK_STACK`

```c
#define IPERF_TRAFFIC_TASK_STACK 4096
```


