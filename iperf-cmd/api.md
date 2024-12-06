# API Reference

## Header files

- [iperf_cmd.h](#file-iperf_cmdh)

## File iperf_cmd.h






## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**iperf\_cmd\_register\_iperf**](#function-iperf_cmd_register_iperf) (void) <br>_Registers console commands: iperf._ |
|  esp\_err\_t | [**iperf\_cmd\_register\_report\_handler**](#function-iperf_cmd_register_report_handler) (iperf\_report\_handler\_func\_t report\_hndl, void \*priv) <br>_Register user's report callback function._ |



## Functions Documentation

### function `iperf_cmd_register_iperf`

_Registers console commands: iperf._
```c
esp_err_t iperf_cmd_register_iperf (
    void
) 
```


**Returns:**

ESP\_OK on success
### function `iperf_cmd_register_report_handler`

_Register user's report callback function._
```c
esp_err_t iperf_cmd_register_report_handler (
    iperf_report_handler_func_t report_hndl,
    void *priv
) 
```


**Note:**

Registered function is then passed to iperf instances as configuration parameter at their startup.



**Parameters:**


* `report_hndl` report handler function to process runtime details from iperf 
* `priv` pointer to user's private data later passed to report function 


**Returns:**



* ESP\_OK on success


