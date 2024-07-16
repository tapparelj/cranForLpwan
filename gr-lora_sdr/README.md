## Summary
This is a modified version of the fully-functional GNU Radio software-defined radio (SDR) implementation of a LoRa transceiver available [here](https://github.com/tapparelj/gr-lora_sdr). This version is modified to be used with the C-RAN implementation .This work has been conducted at the Telecommunication Circuits Laboratory, EPFL. 

## Functionalities
This OOT module is not planned to be used on its own. It should be used with the C-RAN architecture.
See the original repository [gr-lora_sdr](https://github.com/tapparelj/gr-lora_sdr) for a standalone LoRa receiver.


## Reference
J. Tapparel, O. Afisiadis, P. Mayoraz, A. Balatsoukas-Stimming and A. Burg, "An Open-Source LoRa Physical Layer Prototype on GNU Radio," 2020 IEEE 21st International Workshop on Signal Processing Advances in Wireless Communications (SPAWC), Atlanta, GA, USA, 2020, pp. 1-5.

[IEEE Xplore link](https://ieeexplore.ieee.org/document/9154273), [arXiv link](https://arxiv.org/abs/2002.08208)

If you find this implementation useful for your project, please consider citing the aforementioned paper.

## Prerequisites
- Gnuradio 3.10
- python 3
- cmake
- libvolk
- Boost
- UHD
- gcc > 9.3.0
- gxx
- pybind11

## Frequent issues:  
- "ImportError: No module named lora_sdr":
	- This issue comes probably from missing PYTHONPATH and LD_LIBRARY_PATH                             
	- Refer to https://wiki.gnuradio.org/index.php/ModuleNotFoundError to modify those variables. If you set a prefix during the "cmake" call, skip directly to point C.(Verifying that the paths exist in your folders might help.)
- The OOT blocks doesn't appear in gnuradio-companion:	
	- The new blocks can be loaded in gnuradio-companion by adding the following lines in home/{username}/.gnuradio/config.conf (If this file doesn't exist you need to create it):
		```
		[grc]
		local_blocks_path=path_to_the_downloaded_folder/gr-lora_sdr/grc

## Licence
Distributed under the GPL-3.0 License License. See LICENSE for more information.




	














