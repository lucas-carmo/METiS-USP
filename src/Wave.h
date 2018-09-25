#pragma once

#include <string>
#include <armadillo>

using namespace arma; // For armadillo classes

class Wave{
private:
    double m_height;
    double m_period;
    double m_direction;

	// Since the wave number and length are directly related to the wave period by the 
	// dispersion relation, having them as members is a violation of the DRY principle.
	// However, as calculating them every time step would be a large waste of time, I 
	// decided to keep them and to avoid any setter, in such a way that it is necessary 
	// to use the constructors to set the parameters.
	//
	// In any case, they receive NaN as default value. If the user tries to call
	// ENVIR.waveNumber() or ENVIR.length() and they are NaN, an exception is thrown.
	// Nevertheless, you can call ENVIR.waveNumber(watDepth, gravity).
	double m_waveNumber = arma::datum::nan;
	double m_length = arma::datum::nan;

	// Parameters for the numerical solution of the dispersion equation
	static double constexpr m_epsWave = 1e-4;
	static int constexpr m_maxIterWave = 100;

public:
	/*****************************************************
		Constructors
	*****************************************************/
	Wave(double height, double period, double direction, double watDepth, double gravity);

	Wave(const std::string &wholeWaveLine);


	/*****************************************************
		Getters
	*****************************************************/
	double height() const;
	double period() const;
	double direction() const;
	double freq() const;
	double angFreq() const;

	double waveNumber() const;
	double length() const;
	double waveNumber(const double watDepth, const double gravity) const;
	double length(const double watDepth, const double gravity) const;

	/*****************************************************
		Other functions
	*****************************************************/
	// vec::fixed<3> Wave::fluidVel(ENVIR &envir, vec &point) const;
	// vec::fixed<3> fluidAcc(ENVIR &envir, vec &point) const;	
};
