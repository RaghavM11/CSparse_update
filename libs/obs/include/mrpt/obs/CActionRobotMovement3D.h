/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */
#pragma once

#include <mrpt/obs/CAction.h>
#include <mrpt/poses/CPose3DPDFGaussian.h>

namespace mrpt::obs
{
/** Represents a probabilistic 3D (6D) movement.
 *   Currently this can be determined from visual odometry for full 6D, or from
 * wheel encoders for 2D movements only.
 * Here implemented the  motion model from the next article: A. L. Ballardini,
 * A. Furlan, A. Galbiati, M. Matteucci, F. Sacchi, D. G. Sorrenti An effective
 * 6DoF motion model for 3D-6DoF Monte Carlo Localization 4th Workshop on
 * Planning, Perception and Navigation for Intelligent Vehicles, IROS, 2012
 * \ingroup mrpt_obs_grp
 * \sa CAction
 */
class CActionRobotMovement3D : public CAction
{
	DEFINE_SERIALIZABLE(CActionRobotMovement3D, mrpt::obs)

   public:
	/** A list of posible ways for estimating the content of a
	 * CActionRobotMovement3D object.
	 */
	enum TEstimationMethod
	{
		emOdometry = 0,
		emVisualOdometry
	};

	CActionRobotMovement3D() = default;

	/** The 3D pose change probabilistic estimation. It can be converted to/from
	 * these alternative classes:
	 * - mrpt::poses::CPose3DQuatPDFGaussian
	 */
	mrpt::poses::CPose3DPDFGaussian poseChange{};

	/** This is the raw odometry reading, and only is used when
	 * "estimationMethod" is "TEstimationMethod::emOdometry" */
	mrpt::poses::CPose3D rawOdometryIncrementReading{};

	/** This fields indicates the way this estimation was obtained */
	TEstimationMethod estimationMethod{emOdometry};

	enum TDrawSampleMotionModel
	{
		mmGaussian = 0,
		mm6DOF
	};

	/** The parameter to be passed to "computeFromOdometry".
	 * Based ont he motion model:
	 * A. L. Ballardini, A. Furlan, A. Galbiati, M. Matteucci, F. Sacchi, D.
	 * G. Sorrenti, "An effective 6DoF motion model for 3D-6DoF Monte Carlo
	 * Localization", 4th Workshop on Planning, Perception and Navigation for
	 * Intelligent Vehicles, IROS, 2012
	 */
	struct TMotionModelOptions
	{
		TMotionModelOptions() = default;

		/** The model to be used. */
		TDrawSampleMotionModel modelSelection{mm6DOF};

		struct TOptions_6DOFModel
		{
			/** Options for the 6DOFModel model  which generates a
			 * CPosePDFParticles object an then create from that
			 * CPosePDFGaussian object  in poseChange */
			uint32_t nParticlesCount{300};
			float a1{0}, a2{0}, a3{0}, a4{0}, a5{0}, a6{0}, a7{0}, a8{0}, a9{0},
				a10{0};

			/** An additional noise added to the 6DOF model (std. dev. in meters
			 * and radians). */
			float additional_std_XYZ{0.001f},
				additional_std_angle{mrpt::DEG2RAD(0.05f)};
		};

		TOptions_6DOFModel mm6DOFModel;

	} motionModelConfiguration;

	/** Computes the PDF of the pose increment from an odometry reading and
	 * according to the given motion model (speed and encoder ticks information
	 * is not modified).
	 * According to the parameters in the passed struct, it will be called one
	 * the private sampling functions (see "see also" next).
	 * \sa computeFromOdometry_model6DOF
	 */
	void computeFromOdometry(
		const mrpt::poses::CPose3D& odometryIncrement,
		const TMotionModelOptions& options);
	/** Computes the PDF of the pose increment from an odometry reading, using
	 * the motion model for 6 DOF.
	 *  The source: A. L. Ballardini, A. Furlan, A. Galbiati, M. Matteucci, F.
	 * Sacchi, D. G. Sorrenti An effective 6DoF motion model for 3D-6DoF Monte
	 * Carlo Localization 4th Workshop on Planning, Perception and Navigation
	 * for Intelligent Vehicles, IROS, 2012
	 * \sa computeFromOdometry
	 */
	void computeFromOdometry_model6DOF(
		const mrpt::poses::CPose3D& odometryIncrement,
		const TMotionModelOptions& o);

	virtual void getDescriptionAsText(std::ostream& o) const override;

	/** Each "true" entry means that the corresponding "velocities" element
	 * contains valid data - There are 6 entries.
	 */
	std::vector<bool> hasVelocities{false, false, false, false, false, false};

	/** The velocity of the robot in each of 6D:
	 * v_x,v_y,v_z,v_yaw,v_pitch,v_roll (linear in meters/sec and angular in
	 * rad/sec).
	 */
	mrpt::math::CVectorFloat velocities{6};

};	// End of class def.

}  // namespace mrpt::obs
