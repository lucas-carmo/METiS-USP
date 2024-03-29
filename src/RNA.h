#pragma once

#include "Blade.h"
#include "Airfoil.h"
#include <vector>

class RNA
{
private:
    double m_rotorSpeed;
    double m_rotorTilt;
    double m_rotorYaw;
    std::vector< Blade > m_blades;
    std::vector< Airfoil > m_airfoils;
    double m_hubRadius;
    double m_hubHeight;
	double m_hubHeight2CoG; // z coordinate of the hub in the FOWT coordinate system. It is equal to the relative height in t=0.
    double m_overhang;

    bool m_useTipLoss;
    bool m_useHubLoss;
    bool m_useSkewCorr;

public:
	RNA();

	/*****************************************************
		Setters
	*****************************************************/
	void setUseTipLoss(const bool useTipLoss);
	void setUseHubLoss(const bool useHubLoss);
	void setUseSkewCorr(const bool useSkewCorr);

	void setRotorSpeed(const double rotorSpeed);
	void setRotorTilt(const double rotorTilt);
	void setRotorYaw(const double rotorYaw);

	void setNumBlades(const unsigned int numBlades);
	void setBladePrecone(const double precone);
	void setBladePitch(const double pitch);
  
	void setHubRadius(const double hubRadius);
	void setHubHeight(const double hubHeight);
	void setOverhang(const double overhang);
	void setHubHeight2CoG(const double zCoG);

	void addBladeAeroNode(const double span, const double crvAC, const double swpAC, const double crvAng, const double twist, const double chord, const int airfoilID);
	void addEmptyAirfoil();
	void addAirfoilData(const double angle, const double CL, const double CD, const double CM);

	/*****************************************************
		Getters
	*****************************************************/
	double rotorSpeed() const;
	double rotorTilt() const;
	double rotorYaw() const;
	double hubRadius() const;
	double hubHeight() const;
	double overhang() const;
	double hubHeight2CoG() const;
	unsigned int numBlades() const;
	double bladePrecone(const unsigned int indexBlade) const;
	double bladePitch(const unsigned int indexBlade) const;
	bool useTipLoss() const;
	bool useHubLoss() const;
	bool useSkewCorr() const;

	std::string printBladeAero() const;
	std::string printAirfoils() const;

	/*****************************************************
		Caculation functions
	*****************************************************/
	double dAzimuth(const double time) const;

	vec::fixed<6> aeroForce(const ENVIR &envir, const vec::fixed<6> &FOWTpos, const vec::fixed<6> &FOWTvel);

	// Functions for the BEMT method
	double Brent(const double phi_min, const double phi_max, const unsigned int bladeIndex, const unsigned int nodeIndex, const double localSolidity, const double localTipSpeed) const;
	double calcRes(const double phi, const unsigned int bladeIndex, const unsigned int nodeIndex, const double localSolidity, const double localTipSpeed) const;
	double Cn(const double phi, const unsigned int bladeIndex, const unsigned int nodeIndex) const;
	double Ct(const double phi, const unsigned int bladeIndex, const unsigned int nodeIndex) const;
	double Cm(const double phi, const unsigned int bladeIndex, const unsigned int nodeIndex) const;
	double calcF(const double phi, const int nodeIndex) const;
	double calcK(const double phi, const double localSolidity, const double Cn, const double F) const;
	double calcKp(const double phi, const double localSolidity, const double Ct, const double F) const;
	double calcAxialIndFactor(const double k, const double phi, const double F) const;
	double calcTangIndFactor(const double kp, const double F) const;
};
