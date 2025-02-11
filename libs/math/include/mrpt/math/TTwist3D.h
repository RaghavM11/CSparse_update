/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */
#pragma once

#include <mrpt/math/TPoseOrPoint.h>

namespace mrpt::math
{
/** 3D twist: 3D velocity vector (vx,vy,vz) + angular velocity (wx,wy,wz)
 * \sa mrpt::math::TTwist2D, mrpt::math::TPose3D
 * \ingroup geometry_grp
 */
struct TTwist3D : public internal::ProvideStaticResize<TTwist3D>
{
	enum
	{
		static_size = 6
	};

	/** Velocity components: X,Y (m/s) */
	double vx{.0}, vy{.0}, vz{.0};
	/** Angular velocity (rad/s) */
	double wx{.0}, wy{.0}, wz{.0};

	/** Constructor from components */
	constexpr TTwist3D(
		double vx_, double vy_, double vz_, double wx_, double wy_, double wz_)
		: vx(vx_), vy(vy_), vz(vz_), wx(wx_), wy(wy_), wz(wz_)
	{
	}
	/** Default fast constructor. Initializes to zeros  */
	TTwist3D() = default;

	/** Builds from the first 6 elements of a vector-like object: [vx vy vz wx
	 * wy wz]
	 *
	 * \tparam Vector It can be std::vector<double>, Eigen::VectorXd, etc.
	 */
	template <typename Vector>
	static TTwist3D FromVector(const Vector& v)
	{
		TTwist3D o;
		for (int i = 0; i < 6; i++)
			o[i] = v[i];
		return o;
	}

	/** Coordinate access using operator[]. Order: vx,vy,vz, wx, wy, wz */
	double& operator[](size_t i)
	{
		switch (i)
		{
			case 0: return vx;
			case 1: return vy;
			case 2: return vz;
			case 3: return wx;
			case 4: return wy;
			case 5: return wz;
			default: throw std::out_of_range("index out of range");
		}
	}
	/// \overload
	constexpr double operator[](size_t i) const
	{
		switch (i)
		{
			case 0: return vx;
			case 1: return vy;
			case 2: return vz;
			case 3: return wx;
			case 4: return wy;
			case 5: return wz;
			default: throw std::out_of_range("index out of range");
		}
	}

	/** (i,0) access operator (provided for API compatibility with matrices).
	 * \sa operator[] */
	double& operator()(int row, int col)
	{
		ASSERT_EQUAL_(col, 0);
		return (*this)[row];
	}
	/// \overload
	constexpr double operator()(int row, int col) const
	{
		ASSERT_EQUAL_(col, 0);
		return (*this)[row];
	}

	/** Scale factor */
	void operator*=(const double k)
	{
		vx *= k;
		vy *= k;
		vz *= k;
		wx *= k;
		wy *= k;
		wz *= k;
	}

	/** Transformation into vector [vx vy vz wx wy wz].
	 * \tparam Vector It can be std::vector<double>, Eigen::VectorXd, etc.
	 */
	template <typename Vector>
	void asVector(Vector& v) const
	{
		v.resize(6);
		for (int i = 0; i < 6; i++)
			v[i] = (*this)[i];
	}
	/// \overload
	template <typename Vector>
	Vector asVector() const
	{
		Vector v;
		asVector(v);
		return v;
	}

	/** Sets from a vector [vx vy vz wx wy wz] */
	template <typename VECTORLIKE>
	void fromVector(const VECTORLIKE& v)
	{
		ASSERT_EQUAL_(v.size(), 6);
		for (int i = 0; i < 6; i++)
			(*this)[i] = v[i];
	}
	bool operator==(const TTwist3D& o) const;
	bool operator!=(const TTwist3D& o) const;
	/** Returns a human-readable textual representation of the object (eg: "[vx
	 * vy vz wx wy wz]", omegas in deg/s)
	 * \sa fromString
	 */
	void asString(std::string& s) const;
	std::string asString() const
	{
		std::string s;
		asString(s);
		return s;
	}

	/** Transform all 6 components for a change of reference frame from "A" to
	 * another frame "B" whose rotation with respect to "A" is given by `rot`.
	 * The translational part of the pose is ignored */
	void rotate(const mrpt::math::TPose3D& rot);

	/** Like rotate(), but returning a copy of the rotated twist.
	 *  \note New in MRPT 2.3.2 */
	[[nodiscard]] TTwist3D rotated(const mrpt::math::TPose3D& rot) const
	{
		TTwist3D r = *this;
		r.rotate(rot);
		return r;
	}

	/** Set the current object value from a string generated by 'asString' (eg:
	 * "[vx vy vz wx wy wz]" )
	 * \sa asString
	 * \exception std::exception On invalid format
	 */
	void fromString(const std::string& s);

	static TTwist3D FromString(const std::string& s)
	{
		TTwist3D o;
		o.fromString(s);
		return o;
	}
};

mrpt::serialization::CArchive& operator>>(
	mrpt::serialization::CArchive& in, mrpt::math::TTwist3D& o);
mrpt::serialization::CArchive& operator<<(
	mrpt::serialization::CArchive& out, const mrpt::math::TTwist3D& o);

}  // namespace mrpt::math

namespace mrpt::typemeta
{
// Specialization must occur in the same namespace
MRPT_DECLARE_TTYPENAME_NO_NAMESPACE(TTwist3D, mrpt::math)

}  // namespace mrpt::typemeta
