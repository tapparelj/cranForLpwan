# Code is being cleaned and will be available before the 19th of July
## Description
This project contains the components of a CRAN implementation for LPWANs. It includes the support of the LoRa modulation compatible with commercial LoRa devices. The architecture has been implemented to have a centralized server that executes the baseband unit (BBU) and the application server. The RRH can be built and executed on a Raspberry Pi.
![arch_overview-1024x322](https://github.com/tapparelj/cranForLpwan/assets/66671413/46cf7b18-fdcc-47d7-969d-93d64353f8c7)
### File Content
- The GNU Radio implementation of LoRa detection is located in the gr-lora_sdr folder
- The GNU Radio implementation of the components of the C-RAN architecture are in the gr-cran folder
- The implementation of the different parts of the C-RAN architecture that utilize the GNU Radio mules is in the framework folder
### Usage
1. Set the configuration according to your setup inside framework/config.hpp
1. Build the gr-cran module and the executables
1. From the framework/bin folder, start the BBU with ./bbu and the application server with ./application
1. On the Raspberry Pi, start the rrh with ./raspi_single_channel_rrh
1. You can now receive frame from LoRa devices
### Installation
1. Download the project code here  (ToDo Add code when cleaning is done)
1. You should first install both GNU Radio OOT modules gr-lora_sdr and gr-cran
    1. Create a build folder at the root of the OOT module
    1. From the build folder call cmake ..
    1. Build the module with make
    1. Install the module with sudo make install
      
1. You can then build the CRAN executables
    1. Create a build folder in framework
    1. From the build folder call cmake .. -DRASPI= <ON/OFF> (for building for the Raspberry Pi RRH or the server)
    1. Build the module with make
    1. If everything went well you should have some executables generated inside the framework/bin folder bbu, raspi_single_channel_rrh, and application
