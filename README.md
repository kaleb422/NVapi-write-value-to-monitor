# NVapi-write-value-to-monitor
Send commands to monitor over i2c using NVapi. <br>
This can be used to issue VCP commands or other manufacturer specific commands


This program relies on the NVIDIA API (NVAPI), to compile it you will need to download the api which can be found here: <br> https://developer.nvidia.com/rtx/path-tracing/nvapi/get-started

### History 
This program was created after discovering that my display does not work with <b>ControlMyMonitor</b> to change inputs using VCP commands. Searching for an antlernative lead me to this thread https://github.com/rockowitz/ddcutil/issues/100 where other users had found a way to switch the inputs of their LG monitors using a linux program, I needed a windows solution. That lead to the NVIDIA API, this program is an adaptation of the i2c example code provided in the API

## Usage

### Syntax
```
writeValueToDisplay.exe <display_index> <input_value> <command_code> [register_address]
```

| Argument | Description |
| -------- | ----------- |
| display_index | Index assigned to monitor by OS (0 for first screen) |
| input_value   | value to write to screen |
| command_code  | VCP code or other|
| register_address | Address to write to, default 0x51 for VCP codes |



## Example Usage
Change display 0 brightness to 50% using VCP code 0x10
```
writeValueToDisplay.exe 0 0x32 0x10 
```
<br>

Change display 0 input to HDMI 1 using VCP code 0x60 on supported displays
```
writeValueToDisplay.exe 0 0x11 0x60 
```

### Change input on some displays
Some displays do not support using VCP codes to change inputs. I have tested this using values from this thread https://github.com/rockowitz/ddcutil/issues/100 with my LG Ultragear 27GP850-B. Your milage may vary with other monitors, <b>use at your own risk!</b>

#### Change input to HDMI 1 on LG Ultragear 27GP850-B
NOTE: LG Ultragear 27GP850-B is display 0 for me
```
writeValueToDisplay.exe 0 0x90 0xF4 0x50
```

#### Change input to Displayport on LG Ultragear 27GP850-B
NOTE: LG Ultragear 27GP850-B is display 0 for me
```
writeValueToDisplay.exe 0 0xD0 0xF4 0x50
```
