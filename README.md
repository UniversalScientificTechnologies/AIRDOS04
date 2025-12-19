# AIRDOS04 - Specialized Cosmic Radiation Dosimeter for Airliners

![AIRDOS04](doc/img/AIRDOS04.jpg)

The upgraded version of [AIRDOS02](https://github.com/UniversalScientificTechnologies/AIRDOS02) airborne detector. The instrument is designed primarily for the dosimetry of cabin crew and flight attendants of commercial flights. A detailed description of the use is in the [user manual](https://docs.dos.ust.cz/airdos/AIRDOS04).

## Design features 

  - Rechargeable accumulators integrated with data storage in the hot-swap module called [BATDATUNIT01](https://docs.dos.ust.cz/airdos/BATDATUNIT01)
    - The changeable module allows new possible features in the future, like GNSS-based position logging with a revised module 
  - One month of continuous logging for a single accumulator charge
  - Easy measurement activation by insertion of the BATDATUNIT module
  - Data download without special software (Mass storage media)
  - Continuously incrementing time marks (Unique logging time from every measurement) 

## Where to get it?

Purchase AIRDOS04, the open-source device designed for versatility, from [Universal Scientific Technologies s.r.o.](https://www.ust.cz). For sales inquiries, contact sale@ust.cz. Developed and maintained by UST, it comes with full commercial availability and dedicated technical support. The device is released under the GPLv3 license.

## Technical parameters

  * Silicon PIN diode detector with 44 mm³ detection volume
  * The effective number of energy channels is 886 ± 3
  * Deposited energy ranges from 36 keV to 7 MeV
  * Energy measurement resolution 15 ± 2 keV
  * Service interface:  USB-C connector (charging and data download)
  * Energy source: 18650 Li-ion cells conforms with [≤ 100 Wh / 2g restrictions](https://www.iata.org/contentassets/6fea26dd84d24b26a7a1fd5788561d6e/passenger-lithium-battery.pdf)
  * Radiation spectra integration time 10 s
  * Maintenance interval (data download, battery recharge)  30 days
  * Maintenance duration under 5 minutes
  * Approx size LxWxH 166 mm x 107 mm x 57 mm (two  of these should fit into the Aircraft printed manual bay)
  * Weight is 0.88 kg (With five accumulator cells in [BATDATUNIT01](https://github.com/mlab-modules/BATDATUNIT01))
  * Environmental operational conditions
    * Device protection: IP30 rating (fully assembled)
    * Operation temperature range: 0°C to 50°C (32°F to 122°F)
    * Operation humidity conditions: non-condensing, 20% to 80% RH

## Example data

![AIRDOS04 radiation spectra measurement](doc/img/AIRDOS04_radiation_spectra.png)

![AIRDOS04 flight dose rate measurement](doc/img/AIRDOS04_doserate.png)
