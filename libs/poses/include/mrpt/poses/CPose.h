/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */
#pragma once

#include <mrpt/poses/CPoseOrPoint.h>

namespace mrpt::poses
{
/** A base class for representing a pose in 2D or 3D.
 *   For more information refer to the <a
 * href="http://www.mrpt.org/2D_3D_Geometry"> 2D/3D Geometry tutorial</a>
 * online.
 * \note This class is based on the CRTP design pattern
 * \sa CPoseOrPoint, CPoint
 * \ingroup poses_grp
 */
template <class DERIVEDCLASS, std::size_t DIM>
class CPose : public CPoseOrPoint<DERIVEDCLASS, DIM>
{
};	// End of class def.

}  // namespace mrpt::poses
