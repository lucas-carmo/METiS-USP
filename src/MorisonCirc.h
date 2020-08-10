#pragma once

#include <armadillo>
#include <string>
#include "MorisonElement.h"


using namespace arma;

class MorisonCirc : public MorisonElement
{
private:
	double m_diam;
	double m_CD;
	double m_CM;

	double m_botDiam;
	double m_topDiam;

public:
	/*****************************************************
		Constructors
	*****************************************************/
	MorisonCirc(const vec &node1Pos, const vec &node2Pos, const vec &cog, const int numIntPoints,
				const bool botPressFlag, const double axialCD, const double axialCa,
				const double diam, const double CD, double CM, const double botDiam, const double topDiam);

	/*****************************************************
		Forces acting on the Morison Element and added mass
	*****************************************************/
	virtual void make_local_base(arma::vec::fixed<3> &xvec, arma::vec::fixed<3> &yvec, arma::vec::fixed<3> &zvec) const override;
	virtual void make_local_base_t0(arma::vec::fixed<3> &xvec, arma::vec::fixed<3> &yvec, arma::vec::fixed<3> &zvec) const override;
	virtual void make_local_base_sd(arma::vec::fixed<3> &xvec, arma::vec::fixed<3> &yvec, arma::vec::fixed<3> &zvec) const override;
	virtual mat::fixed<6, 6> addedMass_perp(const double rho, const vec::fixed<3> &refPt, const int hydroMode) const override;
	virtual double A_perp(const int ii, const int jj, const vec::fixed<3> &x, const vec::fixed<3> &xG, const vec::fixed<3> &xvec, const vec::fixed<3> &yvec) const override;
	virtual mat::fixed<6, 6> addedMass_paral(const double rho, const vec::fixed<3> &refPt, const int hydroMode) const override;
	virtual vec::fixed<6> hydrostaticForce(const double rho, const double g) const override;
	virtual vec::fixed<6> hydrodynamicForce(const ENVIR &envir, const int hydroMode, const mat::fixed<3, 3> &rotat, const vec::fixed<3> &refPt, const vec::fixed<3> &refPt_sd,
												vec::fixed<6> &force_inertia, vec::fixed<6> &force_drag, vec::fixed<6> &force_froudeKrylov,
												vec::fixed<6> &force_inertia_2nd_part1, vec::fixed<6> &force_inertia_2nd_part2, vec::fixed<6> &force_inertia_2nd_part3, 
												vec::fixed<6> &force_inertia_2nd_part4, vec::fixed<6> &force_inertia_2nd_part5) const override;

	/*****************************************************
		Printing
	*****************************************************/
	virtual std::string print() const override;

	/*****************************************************
		Clone for creating copies of the Morison Element
	*****************************************************/
	virtual MorisonCirc* clone() const override;
};