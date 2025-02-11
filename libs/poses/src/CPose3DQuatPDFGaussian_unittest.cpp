/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */

#include <CTraitsTest.h>
#include <gtest/gtest.h>
#include <mrpt/math/num_jacobian.h>
#include <mrpt/math/transform_gaussian.h>
#include <mrpt/poses/CPose3D.h>
#include <mrpt/poses/CPose3DPDFGaussian.h>
#include <mrpt/poses/CPose3DQuatPDFGaussian.h>
#include <mrpt/random.h>

#include <Eigen/Dense>

using namespace mrpt;
using namespace mrpt::poses;
using namespace mrpt::math;
using namespace std;

template class mrpt::CTraitsTest<CPose3DQuatPDFGaussian>;

class Pose3DQuatPDFGaussTests : public ::testing::Test
{
   protected:
	void SetUp() override {}
	void TearDown() override {}
	static CPose3DQuatPDFGaussian generateRandomPoseQuat3DPDF(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale)
	{
		return CPose3DQuatPDFGaussian(
			generateRandomPose3DPDF(x, y, z, yaw, pitch, roll, std_scale));
	}

	static CPose3DPDFGaussian generateRandomPose3DPDF(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale)
	{
		CMatrixDouble61 r;
		mrpt::random::getRandomGenerator().drawGaussian1DMatrix(
			r, 0, std_scale);
		CMatrixDouble66 cov;
		cov.matProductOf_AAt(r);  // random semi-definite positive matrix:
		for (int i = 0; i < 6; i++)
			cov(i, i) += 1e-7;
		CPose3DPDFGaussian p6pdf(CPose3D(x, y, z, yaw, pitch, roll), cov);
		return p6pdf;
	}

	void test_toFromYPRGauss(double yaw, double pitch, double roll)
	{
		// Random pose:
		CPose3DPDFGaussian p1ypr =
			generateRandomPose3DPDF(1.0, 2.0, 3.0, yaw, pitch, roll, 0.1);
		CPose3DQuatPDFGaussian p1quat = CPose3DQuatPDFGaussian(p1ypr);

		// Convert back to a 6x6 representation:
		CPose3DPDFGaussian p2ypr = CPose3DPDFGaussian(p1quat);

		EXPECT_NEAR(0, (p2ypr.cov - p1ypr.cov).array().abs().maxCoeff(), 1e-6)
			<< "p1ypr: " << endl
			<< p1ypr << endl
			<< "p1quat : " << endl
			<< p1quat << endl
			<< "p2ypr : " << endl
			<< p2ypr << endl;
	}

	static void func_compose(
		const CVectorFixedDouble<2 * 7>& x,
		[[maybe_unused]] const double& dummy, CVectorFixedDouble<7>& Y)
	{
		const CPose3DQuat p1(
			x[0], x[1], x[2], CQuaternionDouble(x[3], x[4], x[5], x[6]));
		const CPose3DQuat p2(
			x[7 + 0], x[7 + 1], x[7 + 2],
			CQuaternionDouble(x[7 + 3], x[7 + 4], x[7 + 5], x[7 + 6]));
		const CPose3DQuat p = p1 + p2;
		for (int i = 0; i < 7; i++)
			Y[i] = p[i];
	}

	static void func_inv_compose(
		const CVectorFixedDouble<2 * 7>& x,
		[[maybe_unused]] const double& dummy, CVectorFixedDouble<7>& Y)
	{
		CQuaternionDouble q(x[3], x[4], x[5], x[6]);
		q.normalize();
		const CPose3DQuat p1(x[0], x[1], x[2], q);
		const CPose3DQuat p2(
			x[7 + 0], x[7 + 1], x[7 + 2],
			CQuaternionDouble(x[7 + 3], x[7 + 4], x[7 + 5], x[7 + 6]));
		const CPose3DQuat p = p1 - p2;
		for (int i = 0; i < 7; i++)
			Y[i] = p[i];
	}

	void testPoseComposition(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale, double x2, double y2, double z2, double yaw2,
		double pitch2, double roll2, double std_scale2)
	{
		CPose3DQuatPDFGaussian p7pdf1 =
			generateRandomPoseQuat3DPDF(x, y, z, yaw, pitch, roll, std_scale);
		CPose3DQuatPDFGaussian p7pdf2 = generateRandomPoseQuat3DPDF(
			x2, y2, z2, yaw2, pitch2, roll2, std_scale2);

		CPose3DQuatPDFGaussian p7_comp = p7pdf1 + p7pdf2;

		// Numeric approximation:
		CVectorFixedDouble<7> y_mean;
		CMatrixFixed<double, 7, 7> y_cov;
		{
			CVectorFixedDouble<2 * 7> x_mean;
			for (int i = 0; i < 7; i++)
				x_mean[i] = p7pdf1.mean[i];
			for (int i = 0; i < 7; i++)
				x_mean[7 + i] = p7pdf2.mean[i];

			CMatrixFixed<double, 14, 14> x_cov;
			x_cov.insertMatrix(0, 0, p7pdf1.cov);
			x_cov.insertMatrix(7, 7, p7pdf2.cov);

			double DUMMY = 0;
			CVectorFixedDouble<2 * 7> x_incrs;
			x_incrs.fill(1e-6);
			transform_gaussian_linear(
				x_mean, x_cov, func_compose, DUMMY, y_mean, y_cov, x_incrs);
		}
		// Compare:
		EXPECT_NEAR(0, (y_cov - p7_comp.cov).array().abs().maxCoeff(), 1e-3)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "p2 mean: " << p7pdf2.mean << endl
			<< "Numeric approximation of covariance: " << endl
			<< y_cov << endl
			<< "Returned covariance: " << endl
			<< p7_comp.cov << endl;
	}

	static void func_inverse(
		const CVectorFixedDouble<7>& x, [[maybe_unused]] const double& dummy,
		CVectorFixedDouble<7>& Y)
	{
		CQuaternionDouble q(x[3], x[4], x[5], x[6]);
		q.normalize();
		const CPose3DQuat p1(x[0], x[1], x[2], q);
		const CPose3DQuat p1_inv(-p1);
		for (int i = 0; i < 7; i++)
			Y[i] = p1_inv[i];
	}

	void testCompositionJacobian(
		double x, double y, double z, double yaw, double pitch, double roll,
		double x2, double y2, double z2, double yaw2, double pitch2,
		double roll2)
	{
		const CPose3DQuat q1(CPose3D(x, y, z, yaw, pitch, roll));
		const CPose3DQuat q2(CPose3D(x2, y2, z2, yaw2, pitch2, roll2));

		// Theoretical Jacobians:
		CMatrixDouble77 df_dx(UNINITIALIZED_MATRIX),
			df_du(UNINITIALIZED_MATRIX);
		CPose3DQuatPDF::jacobiansPoseComposition(
			q1,	 // x
			q2,	 // u
			df_dx, df_du);

		// Numerical approximation:
		CMatrixDouble77 num_df_dx(UNINITIALIZED_MATRIX),
			num_df_du(UNINITIALIZED_MATRIX);
		{
			CVectorFixedDouble<2 * 7> x_mean;
			for (int i = 0; i < 7; i++)
				x_mean[i] = q1[i];
			for (int i = 0; i < 7; i++)
				x_mean[7 + i] = q2[i];

			double DUMMY = 0;
			CVectorFixedDouble<2 * 7> x_incrs;
			x_incrs.fill(1e-7);
			CMatrixDouble numJacobs;
			mrpt::math::estimateJacobian(
				x_mean,
				std::function<void(
					const CVectorFixedDouble<2 * 7>& x, const double& dummy,
					CVectorFixedDouble<7>& Y)>(&func_compose),
				x_incrs, DUMMY, numJacobs);

			num_df_dx = numJacobs.block<7, 7>(0, 0);
			num_df_du = numJacobs.block<7, 7>(0, 7);
		}

		// Compare:
		EXPECT_NEAR(0, (df_dx - num_df_dx).array().abs().maxCoeff(), 1e-6)
			<< "q1: " << q1 << endl
			<< "q2: " << q2 << endl
			<< "Numeric approximation of df_dx: " << endl
			<< num_df_dx << endl
			<< "Implemented method: " << endl
			<< df_dx << endl
			<< "Error: " << endl
			<< df_dx - num_df_dx << endl;

		EXPECT_NEAR(0, (df_du - num_df_du).array().abs().maxCoeff(), 1e-6)
			<< "q1: " << q1 << endl
			<< "q2: " << q2 << endl
			<< "Numeric approximation of df_du: " << endl
			<< num_df_du << endl
			<< "Implemented method: " << endl
			<< df_du << endl
			<< "Error: " << endl
			<< df_du - num_df_du << endl;
	}

	void testInverse(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale)
	{
		CPose3DQuatPDFGaussian p7pdf1 =
			generateRandomPoseQuat3DPDF(x, y, z, yaw, pitch, roll, std_scale);

		CPose3DQuatPDFGaussian p7_inv = -p7pdf1;

		// Numeric approximation:
		CVectorFixedDouble<7> y_mean;
		CMatrixFixed<double, 7, 7> y_cov;
		{
			CVectorFixedDouble<7> x_mean;
			for (int i = 0; i < 7; i++)
				x_mean[i] = p7pdf1.mean[i];

			CMatrixFixed<double, 7, 7> x_cov;
			x_cov.insertMatrix(0, 0, p7pdf1.cov);

			double DUMMY = 0;
			CVectorFixedDouble<7> x_incrs;
			x_incrs.fill(1e-6);
			transform_gaussian_linear(
				x_mean, x_cov, func_inverse, DUMMY, y_mean, y_cov, x_incrs);
		}

		// Compare:
		EXPECT_NEAR(0, (y_cov - p7_inv.cov).array().abs().maxCoeff(), 1e-6)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "inv mean: " << p7_inv.mean << endl
			<< "Numeric approximation of covariance: " << endl
			<< y_cov << endl
			<< "Returned covariance: " << endl
			<< p7_inv.cov << endl
			<< "Error: " << endl
			<< y_cov - p7_inv.cov << endl;
	}

	void testPoseInverseComposition(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale, double x2, double y2, double z2, double yaw2,
		double pitch2, double roll2, double std_scale2)
	{
		CPose3DQuatPDFGaussian p7pdf1 =
			generateRandomPoseQuat3DPDF(x, y, z, yaw, pitch, roll, std_scale);
		CPose3DQuatPDFGaussian p7pdf2 = generateRandomPoseQuat3DPDF(
			x2, y2, z2, yaw2, pitch2, roll2, std_scale2);

		CPose3DQuatPDFGaussian p7_comp = p7pdf1 - p7pdf2;

		// Numeric approximation:
		CVectorFixedDouble<7> y_mean;
		CMatrixFixed<double, 7, 7> y_cov;
		{
			CVectorFixedDouble<2 * 7> x_mean;
			for (int i = 0; i < 7; i++)
				x_mean[i] = p7pdf1.mean[i];
			for (int i = 0; i < 7; i++)
				x_mean[7 + i] = p7pdf2.mean[i];

			CMatrixFixed<double, 14, 14> x_cov;
			x_cov.insertMatrix(0, 0, p7pdf1.cov);
			x_cov.insertMatrix(7, 7, p7pdf2.cov);

			double DUMMY = 0;
			CVectorFixedDouble<2 * 7> x_incrs;
			x_incrs.fill(1e-6);
			transform_gaussian_linear(
				x_mean, x_cov, func_inv_compose, DUMMY, y_mean, y_cov, x_incrs);
		}
		// Compare:
		EXPECT_NEAR(0, (y_cov - p7_comp.cov).array().abs().maxCoeff(), 1e-6)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "p2 mean: " << p7pdf2.mean << endl
			<< "Numeric approximation of covariance: " << endl
			<< y_cov << endl
			<< "Returned covariance: " << endl
			<< p7_comp.cov << endl;
	}

	void testInverseCompositionCrossCorrelation(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale, double x2, double y2, double z2, double yaw2,
		double pitch2, double roll2, double std_scale2)
	{
		CPose3DQuatPDFGaussian p7pdf1 =
			generateRandomPoseQuat3DPDF(x, y, z, yaw, pitch, roll, std_scale);
		CPose3DQuatPDFGaussian p7_displacement = generateRandomPoseQuat3DPDF(
			x2, y2, z2, yaw2, pitch2, roll2, std_scale2);

		CPose3DQuatPDFGaussian p7pdf2 = p7pdf1 + p7_displacement;

		CPose3DQuatPDFGaussian p7_displacement_computed =
			p7pdf1.inverseCompositionCrossCorrelation(p7pdf2);

		// Compare:
		EXPECT_NEAR(
			0,
			(p7_displacement_computed.cov - p7_displacement.cov)
				.array()
				.abs()
				.maxCoeff(),
			1e-6)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "p2 mean: " << p7pdf2.mean << endl
			<< "Computed displacement covariance: " << endl
			<< p7_displacement_computed.cov << endl
			<< "True displacement covariance: " << endl
			<< p7_displacement.cov << endl;
	}

	void testChangeCoordsRef(
		double x, double y, double z, double yaw, double pitch, double roll,
		double std_scale, double x2, double y2, double z2, double yaw2,
		double pitch2, double roll2)
	{
		CPose3DQuatPDFGaussian p7pdf1 =
			generateRandomPoseQuat3DPDF(x, y, z, yaw, pitch, roll, std_scale);

		const CPose3DQuat new_base =
			CPose3DQuat(CPose3D(x2, y2, z2, yaw2, pitch2, roll2));
		const CPose3DQuatPDFGaussian new_base_pdf(
			new_base, CMatrixDouble77());  // COV = Zeros

		const CPose3DQuatPDFGaussian p7_new_base_pdf = new_base_pdf + p7pdf1;
		p7pdf1.changeCoordinatesReference(new_base);

		// Compare:
		EXPECT_NEAR(
			0, (p7_new_base_pdf.cov - p7pdf1.cov).array().abs().maxCoeff(),
			1e-2)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "new_base: " << new_base << endl;
		EXPECT_NEAR(
			0,
			(p7_new_base_pdf.mean.asVectorVal() - p7pdf1.mean.asVectorVal())
				.array()
				.abs()
				.mean(),
			1e-2)
			<< "p1 mean: " << p7pdf1.mean << endl
			<< "new_base: " << new_base << endl;
	}
};

TEST_F(Pose3DQuatPDFGaussTests, ToYPRGaussPDFAndBack)
{
	test_toFromYPRGauss(-30.0_deg, 10.0_deg, 60.0_deg);
	test_toFromYPRGauss(30.0_deg, 88.0_deg, 0.0_deg);
	test_toFromYPRGauss(30.0_deg, 89.5_deg, 0.0_deg);
	// The formulas break at pitch=90, but this we cannot avoid...
}

TEST_F(Pose3DQuatPDFGaussTests, CompositionJacobian)
{
	testCompositionJacobian(
		0, 0, 0, 2.0_deg, 0.0_deg, 0.0_deg, 0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg);
	testCompositionJacobian(
		1, 2, 3, 2.0_deg, 0.0_deg, 0.0_deg, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg);
	testCompositionJacobian(
		1, -2, 3, 2.0_deg, 0.0_deg, 0.0_deg, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg);
	testCompositionJacobian(
		1, 2, -3, 2.0_deg, 0.0_deg, 0.0_deg, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, -8, 45, 10, 50.0_deg, -10.0_deg,
		30.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, -80.0_deg, 70.0_deg, -8, 45, 10, 50.0_deg, -10.0_deg,
		30.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, 80.0_deg, -70.0_deg, -8, 45, 10, 50.0_deg, -10.0_deg,
		30.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, -8, 45, 10, -50.0_deg, -10.0_deg,
		30.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, -8, 45, 10, 50.0_deg, 10.0_deg,
		30.0_deg);
	testCompositionJacobian(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, -8, 45, 10, 50.0_deg, -10.0_deg,
		-30.0_deg);
}

TEST_F(Pose3DQuatPDFGaussTests, Inverse)
{
	testInverse(0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(0, 0, 0, 10.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(0, 0, 0, 0.0_deg, 10.0_deg, 0.0_deg, 0.1);
	testInverse(0, 0, 0, 0.0_deg, 0.0_deg, 10.0_deg, 0.1);

	testInverse(1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.2);

	testInverse(1, 2, 3, 30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, 3, 30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, -3, 30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, -3, 30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, 3, -30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, 3, -30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, -3, -30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, -3, -30.0_deg, 0.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, 3, 0.0_deg, 30.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, 3, 0.0_deg, 30.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, -3, 0.0_deg, 30.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, -3, 0.0_deg, 30.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, 3, 0.0_deg, -30.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, 3, 0.0_deg, -30.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, -3, 0.0_deg, -30.0_deg, 0.0_deg, 0.1);
	testInverse(-1, 2, -3, 0.0_deg, -30.0_deg, 0.0_deg, 0.1);
	testInverse(1, 2, 3, 0.0_deg, 0.0_deg, 30.0_deg, 0.1);
	testInverse(-1, 2, 3, 0.0_deg, 0.0_deg, 30.0_deg, 0.1);
	testInverse(1, 2, -3, 0.0_deg, 0.0_deg, 30.0_deg, 0.1);
	testInverse(-1, 2, -3, 0.0_deg, 0.0_deg, 30.0_deg, 0.1);
	testInverse(1, 2, 3, 0.0_deg, 0.0_deg, -30.0_deg, 0.1);
	testInverse(-1, 2, 3, 0.0_deg, 0.0_deg, -30.0_deg, 0.1);
	testInverse(1, 2, -3, 0.0_deg, 0.0_deg, -30.0_deg, 0.1);
	testInverse(-1, 2, -3, 0.0_deg, 0.0_deg, -30.0_deg, 0.1);
}

TEST_F(Pose3DQuatPDFGaussTests, Composition)
{
	testPoseComposition(
		0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, 0, 0, 0, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);

	testPoseComposition(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.1, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.2, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.2);

	testPoseComposition(
		1, 2, 3, 10.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 10.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 10.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 10.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 10.0_deg,
		0.0_deg, 0.1);
	testPoseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		10.0_deg, 0.1);
}

TEST_F(Pose3DQuatPDFGaussTests, InverseComposition)
{
	testPoseInverseComposition(
		0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, 0, 0, 0, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);

	testPoseInverseComposition(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.1, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.2, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.2);

	testPoseInverseComposition(
		1, 2, 3, 10.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 10.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 10.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 10.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 10.0_deg,
		0.0_deg, 0.1);
	testPoseInverseComposition(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		10.0_deg, 0.1);
}

TEST_F(Pose3DQuatPDFGaussTests, RelativeDisplacement)
{
	testInverseCompositionCrossCorrelation(
		0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, 0, 0, 0, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);

	testInverseCompositionCrossCorrelation(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.1, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.2, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg, 0.2);

	testInverseCompositionCrossCorrelation(
		1, 2, 3, 10.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 10.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 0.0_deg, 10.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 10.0_deg, 0.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 10.0_deg,
		0.0_deg, 0.1);
	testInverseCompositionCrossCorrelation(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		10.0_deg, 0.1);
}

TEST_F(Pose3DQuatPDFGaussTests, ChangeCoordsRef)
{
	testChangeCoordsRef(
		0, 0, 0, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, 0, 0, 0, 0.0_deg, 0.0_deg,
		0.0_deg);
	testChangeCoordsRef(
		1, 2, 3, 0.0_deg, 0.0_deg, 0.0_deg, 0.1, -8, 45, 10, 0.0_deg, 0.0_deg,
		0.0_deg);

	testChangeCoordsRef(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.1, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg);
	testChangeCoordsRef(
		1, 2, 3, 20.0_deg, 80.0_deg, 70.0_deg, 0.2, -8, 45, 10, 50.0_deg,
		-10.0_deg, 30.0_deg);
}
