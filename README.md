Copyright (C) 2014-2015, Western Digital Technologies, Inc. [<copyrightagent@wdc.com>](mailto:copyrightagent@wdc.com>)

# Introduction

SMR Simulator - SMRSim is a library tool that captures statistics for host software behavior and “friendliness” to Shingle Magnetic Recording (SMR) technology. By releasing an SMR simulator, we hope to enable open source developers to experiment and become familiar with SMR functionalities and behaviors without the need to access real SMR (ZBC/ZAC) HW. SMRSim can enforce host managed SMR read/write policies defined by the INCITS technical T10/T13 committees. SMRSim collects host workload statistics and may be configured for various zone sizes.

With host managed SMR device, the host uses commands and Zone information to optimize the behavior of the SMR device by managing IOs to ensure the read/write policies within a Zone. By the default configuration if a host sends an out of policy read/write, SMRSim will reject it and return an error.

## Linux Stack from the Perspective of the Simulator

![Linux stack from the perspective of the simulator.](https://github.com/westerndigitalcorporation/SMR-Simulator/blob/master/documentation/specifications/graphics/linux_stack_from_the_perspective_of_the_simulator.png)

# Software Requirements

*   Linux Kernel 3.14.0 or greater.

# Goals for Linux SMR Simulator

*   Maintain awareness of Out of Policy Writes, Reads, and Device Idle Time.
    *   Out of Policy Writes: does not start on WP, spans zones, or not on 4K alignment.
    *   Reads: do not across WP and does not span zones.
*   Provide a collection of statistics for the items listed above via a collection of ioctls that can be console-printed or saved by the usermode application.
*   Provide a collection of parameters to adjust the behavior of the simulation. These parameters can be provided via ioctls from user mode or as arguments to the simulator constructor.
*   Provide configurable latency for Out of Policy Reads and Writes.

# Non-Goals

*   Exact compliance to ZAC/ZBC specification
*   Support of different sized zones
*   Support of vibration detection/simulation
*   Sense codes reporting
*   Trying to be host aware
*   Zone open/close state tracking

# Design Overview

## Simulated Device

![Simulated Device](https://github.com/westerndigitalcorporation/SMR-Simulator/blob/master/documentation/specifications/graphics/simulated_device.png)

# Overview

## Kernel Module

A Device Mapper kernel module emulates a SMR device and provides a set of ioctls for user applications and function calls for peer kernel modules for configuration and data collection. The data collected are counts on the following:

<dl>
<dt>Out of policy read is a read command that:</dt>
<dd>Starts or goes beyond (Write Pointer) WP</dd>
<dd>Spans zones</dd>
<dt>Out of policy write is a write command that:</dt>
<dd>Does not start on WP</dd>
<dd>Spans zones</dd>
<dd>Is not 4k aligned in SMR zones</dd>
</dl>

## Flow Chart

![Flow Chart](https://github.com/westerndigitalcorporation/SMR-Simulator/blob/master/documentation/specifications/graphics/flow_chart_zacsimkmd_kernel_module.png)

## Tracepoints

Tracepoints are available to help the users of the simulator find out how it works and potentially determine what is non-host manage compliant with their applications.

## Simulator configuration utility

A command line application allows the user to configure the behavior of the SMR simulator and to display the data collected by the simulator.

# Standards Versions Supported

ZAC/ZBC standards are still being developed. Changes to the command set and command interface can be expected before the final public release.

# License

SMRSim is distributed under the terms of GPL v2 or any later version. 

SMRSim and all its example applications are distributed "as is," without technical support, and WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Along with SMRSim, you should have received a copy of the GNU General Public License. If not, please see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

# Contact and Bug Reports

Please contact Shanghua Wang ([Shanghua.Wang@wdc.com](mailto:shanghua.wang@wdc.com)) to report problems.

# References

*   T13 ZAC Zone State Machine et aliae, Revision 6, 05/18/2015.
*   Information technology - Zoned-device ATA Command Set (ZAC), T13/BSR INCITS 537, Revision 02, 04/27/2015.
*   Information technology - Zoned Block Commands (ZBC), T10/536-201x, Revision 0, 03/12/2014.

