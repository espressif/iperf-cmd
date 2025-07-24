# API Reference

## Header files

- [iperf_cmd.h](#file-iperf_cmdh)

## File iperf_cmd.h






## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**iperf\_cmd\_register\_iperf**](#function-iperf_cmd_register_iperf) (void) <br>_Registers console commands: iperf._ |
|  esp\_err\_t | [**iperf\_cmd\_set\_iperf\_state\_handler**](#function-iperf_cmd_set_iperf_state_handler) (iperf\_state\_handler\_func\_t state\_hndl, void \*priv) <br>_Sets the global default iperf state handler. This handler will be passed to each iperf instance as a configuration parameter._ |



## Functions Documentation

### function `iperf_cmd_register_iperf`

_Registers console commands: iperf._
```c
esp_err_t iperf_cmd_register_iperf (
    void
) 
```


**Returns:**



* ESP\_OK on success
### function `iperf_cmd_set_iperf_state_handler`

_Sets the global default iperf state handler. This handler will be passed to each iperf instance as a configuration parameter._
```c
esp_err_t iperf_cmd_set_iperf_state_handler (
    iperf_state_handler_func_t state_hndl,
    void *priv
) 
```


**Note:**

This function should be called before starting the console.



**Parameters:**


* `state_hndl` state handler function to process runtime details from iperf 
* `priv` pointer to user's private data later passed to state handler function 


**Returns:**



* ESP\_OK on success


