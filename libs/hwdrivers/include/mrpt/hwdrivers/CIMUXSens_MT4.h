/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */

#pragma once

#include <mrpt/core/pimpl.h>
#include <mrpt/hwdrivers/CGenericSensor.h>
#include <mrpt/poses/CPose3D.h>

namespace mrpt::hwdrivers
{
/** A class for interfacing XSens 4th generation Inertial Measuring Units
 * (IMUs): MTi 10-series, MTi 100-series.
 *  Usage considerations:
 *    - In Windows, you only need to install XSens drivers.
 *    - In Linux, this class requires the system libraries: libusb-1.0 &
 * libudev (dev packages). Accessing USB devices may require
 *      running the program as super user ("sudo"). To avoid that, Or, install
 * <code> MRPT/scripts/52-xsens.rules </code> in <code>/etc/udev/rules.d/</code>
 * to allow access to all users.
 *
 *  \code
 *  PARAMETERS IN THE ".INI"-LIKE CONFIGURATION STRINGS:
 * -------------------------------------------------------
 *   [supplied_section_name]
 *    pose_x=0	    // Sensor 3D position relative to the robot (meters)
 *    pose_y=0
 *    pose_z=0
 *    pose_yaw=0	// Angles in degrees
 *    pose_pitch=0
 *    pose_roll=0
 *    sensorLabel = <label>   // Label of the sensor
 *    #sampleFreq  = 100  // The requested rate of sensor packets (default:
 * 100Hz)
 *    # If a portname is not provided, the first found device will be opened:
 *    #portname_LIN	= USB002:005
 *    #portname_WIN	= \\?\usb#vid_2639&pid_0003#...
 *    #baudRate	    = 115200   // Baudrate for communicating, only if
 *                             // the port is a COM port
 *    #deviceId     = xxxxx    // Device ID to open, or first one if empty.
 *    #logFile      = xxxx     // If provided, will enable XSens SDK's own log
 *  \endcode
 *
 *  \note Set the environment variable "MRPT_HWDRIVERS_VERBOSE" to "1" to
 * enable diagnostic information while using this class.
 *
 * \ingroup mrpt_hwdrivers_grp
 */
class CIMUXSens_MT4 : public hwdrivers::CGenericSensor
{
	DEFINE_GENERIC_SENSOR(CIMUXSens_MT4)
   public:
	CIMUXSens_MT4();
	~CIMUXSens_MT4() override;

	/** This method will be invoked at a minimum rate of "process_rate" (Hz)
	 *  \exception This method must throw an exception with a descriptive
	 * message if some critical error is found.
	 */
	void doProcess() override;

	/** Turns on the xSens device and configure it for getting orientation data
	 */
	void initialize() override;

	void close();

   protected:
	/** The interface to the file: */
	struct Impl;
	mrpt::pimpl<Impl> m_impl;

	/** Baudrate, only for COM ports. */
	int m_port_bauds{0};
	/** The USB or COM port name (if blank -> autodetect) */
	std::string m_portname;

	/** Device ID to open, or first one if empty string. */
	std::string m_deviceId;

	std::string m_xsensLogFile;

	int m_sampleFreq{100};

	mrpt::poses::CPose3D m_sensorPose;

	/** See the class documentation at the top for expected parameters */
	void loadConfig_sensorSpecific(
		const mrpt::config::CConfigFileBase& configSource,
		const std::string& iniSection) override;

	friend class MyXSensCallback;

};	// end of class

}  // namespace mrpt::hwdrivers
