#include "MorisonCirc.h"
#include "auxFunctions.h"
#include <cmath>


using namespace arma;

/*****************************************************
	Constructors
*****************************************************/
MorisonCirc::MorisonCirc(const vec &node1Pos, const vec &node2Pos, const vec &cog, const int numIntPoints,
	const bool botPressFlag, const double axialCD_1, const double axialCa_1, const double axialCD_2, const double axialCa_2,
	const double diam, const double CD, double CM)
	: MorisonElement(node1Pos, node2Pos, cog, numIntPoints, botPressFlag, axialCD_1, axialCa_1, axialCD_2, axialCa_2),
	m_diam(diam), m_CD(CD), m_CM(CM)
{
	make_local_base(m_xvec_t0, m_yvec_t0, m_zvec_t0, node1Pos, node2Pos);

	m_xvec_sd = m_xvec_t0;
	m_yvec_sd = m_yvec_t0;
	m_zvec_sd = m_zvec_t0;

	m_xvec = m_xvec_t0;
	m_yvec = m_yvec_t0;
	m_zvec = m_zvec_t0;
}

// TODO: There is a lot of code repetition in this function that could be avoided.
void MorisonCirc::evaluateQuantitiesAtBegin(const ENVIR &envir, const int hydroMode)
{
	typedef std::vector<cx_double> cx_stdvec;
	m_flagFixed = true;

	const vec &t = envir.getTimeArray();
	int nWaves = envir.numberOfWaveComponents();

	if (m_node1Pos_sd.at(2) > 0)
		return;

	// Calculate immersed length and its discretization
	double Lw;
	int npts;
	calculateImmersedLengthProperties_sd(Lw, npts, m_dL);

	// Position of each node along the cylinder length
	m_nodesArray = zeros(3, npts);
	for (int iNode = 0; iNode < npts; ++iNode)
	{
		m_nodesArray.col(iNode) = m_node1Pos_sd + iNode * m_dL * m_zvec_sd;
	}

	// Wave forces or kinematics at each node at each time step
	m_hydroForce_1st_Array = zeros(t.size(), 6);
	m_hydroForce_2nd_Array = zeros(t.size(), 6);
	m_waveElevAtWL = zeros(t.size(), 1);
	m_u1_Array_x = zeros(t.size(), npts);
	m_u1_Array_y = zeros(t.size(), npts);
	m_u1_Array_z = zeros(t.size(), npts);

	// Need to store the frequency of each wave in case the IFFT won't be used
	vec w{ zeros(nWaves, 1) };

	// Complex amplitudes	
	cx_mat amp_hydroForce_1st(nWaves, 6, fill::zeros);	
	cx_vec amp_waveElevAtWL(nWaves, 1, fill::zeros);
	cx_mat amp_u1_x(nWaves, npts, fill::zeros);
	cx_mat amp_u1_y(nWaves, npts, fill::zeros);
	cx_mat amp_u1_z(nWaves, npts, fill::zeros);
	for (unsigned int iWave = 0; iWave < nWaves; ++iWave)
	{
		const Wave &wave(envir.getWave(iWave));
		w.at(iWave) = wave.angFreq();
		amp_hydroForce_1st.row(iWave) = hydroForce_1st_coefs(wave, envir.watDensity(), envir.watDepth(), envir.gravity()).st();
		amp_waveElevAtWL.at(iWave) = envir.waveElev_coef(m_nodesArray.at(0, npts - 1), m_nodesArray.at(1, npts - 1), iWave); // Wave elevation is evaluated at the last node, which is the intersection with the water line

		for (int iNode = 0; iNode < npts; ++iNode)
		{
			cx_vec::fixed<3> aux = envir.u1_coef(m_nodesArray.at(0, iNode), m_nodesArray.at(1, iNode), m_nodesArray.at(2, iNode), iWave);
			amp_u1_x.at(iWave, iNode) = aux.at(0);
			amp_u1_y.at(iWave, iNode) = aux.at(1);
			amp_u1_z.at(iWave, iNode) = aux.at(2);
		}		
	}

	m_waveElevAtWL = envir.timeSeriesFromAmp(amp_waveElevAtWL, w);
	m_hydroForce_1st_Array = envir.timeSeriesFromAmp(amp_hydroForce_1st, w);
	m_u1_Array_x = envir.timeSeriesFromAmp(amp_u1_x, w);
	m_u1_Array_y = envir.timeSeriesFromAmp(amp_u1_y, w);
	m_u1_Array_z = envir.timeSeriesFromAmp(amp_u1_z, w);

	// Quantities that are used only in second order analysis
	if (hydroMode == 2)
	{
		m_du1dx_Array_x = zeros(t.size(), npts);
		m_du1dx_Array_y = zeros(t.size(), npts);
		m_du1dx_Array_z = zeros(t.size(), npts);

		m_du1dy_Array_x = zeros(t.size(), npts);
		m_du1dy_Array_y = zeros(t.size(), npts);
		m_du1dy_Array_z = zeros(t.size(), npts);

		m_du1dz_Array_x = zeros(t.size(), npts);
		m_du1dz_Array_y = zeros(t.size(), npts);
		m_du1dz_Array_z = zeros(t.size(), npts);

		cx_mat amp_du1dx_x(nWaves, npts, fill::zeros);
		cx_mat amp_du1dx_y(nWaves, npts, fill::zeros);
		cx_mat amp_du1dx_z(nWaves, npts, fill::zeros);

		cx_mat amp_du1dy_x(nWaves, npts, fill::zeros);
		cx_mat amp_du1dy_y(nWaves, npts, fill::zeros);
		cx_mat amp_du1dy_z(nWaves, npts, fill::zeros);

		cx_mat amp_du1dz_x(nWaves, npts, fill::zeros);
		cx_mat amp_du1dz_y(nWaves, npts, fill::zeros);
		cx_mat amp_du1dz_z(nWaves, npts, fill::zeros);
		for (unsigned int iWave = 0; iWave < nWaves; ++iWave)
		{
			const Wave &wave(envir.getWave(iWave));

			for (int iNode = 0; iNode < npts; ++iNode)
			{
				cx_vec::fixed<3> aux = envir.du1dx_coef(m_nodesArray.at(0, iNode), m_nodesArray.at(1, iNode), m_nodesArray.at(2, iNode), iWave);
				amp_du1dx_x.at(iWave, iNode) = aux.at(0);
				amp_du1dx_y.at(iWave, iNode) = aux.at(1);
				amp_du1dx_z.at(iWave, iNode) = aux.at(2);

				aux = envir.du1dy_coef(m_nodesArray.at(0, iNode), m_nodesArray.at(1, iNode), m_nodesArray.at(2, iNode), iWave);
				amp_du1dy_x.at(iWave, iNode) = aux.at(0);
				amp_du1dy_y.at(iWave, iNode) = aux.at(1);
				amp_du1dy_z.at(iWave, iNode) = aux.at(2);

				aux = envir.du1dz_coef(m_nodesArray.at(0, iNode), m_nodesArray.at(1, iNode), m_nodesArray.at(2, iNode), iWave);
				amp_du1dz_x.at(iWave, iNode) = aux.at(0);
				amp_du1dz_y.at(iWave, iNode) = aux.at(1);
				amp_du1dz_z.at(iWave, iNode) = aux.at(2);
			}
		}

		m_du1dx_Array_x = envir.timeSeriesFromAmp(amp_du1dx_x, w);
		m_du1dx_Array_y = envir.timeSeriesFromAmp(amp_du1dx_y, w);
		m_du1dx_Array_z = envir.timeSeriesFromAmp(amp_du1dx_z, w);

		m_du1dy_Array_x = envir.timeSeriesFromAmp(amp_du1dy_x, w);
		m_du1dy_Array_y = envir.timeSeriesFromAmp(amp_du1dy_y, w);
		m_du1dy_Array_z = envir.timeSeriesFromAmp(amp_du1dy_z, w);

		m_du1dz_Array_x = envir.timeSeriesFromAmp(amp_du1dz_x, w);
		m_du1dz_Array_y = envir.timeSeriesFromAmp(amp_du1dz_y, w);
		m_du1dz_Array_z = envir.timeSeriesFromAmp(amp_du1dz_z, w);
	}

	// Special treatment for the force due to the second-order wave potential due to the double sum
	// See comments in envir.evaluateWaveKinematics() for details
	if (hydroMode == 2)
	{
		if (envir.getFlagIFFT())
		{
			cx_mat amp_hydroForce_2nd(nWaves, 6, fill::zeros);
			for (int iWave = 0; iWave < nWaves; ++iWave)
			{
				const Wave &wave_ii(envir.getWave(iWave));
				if (wave_ii.amp() == 0) continue;

				for (int jWave = 0; jWave <= iWave; ++jWave)
				{
					const Wave &wave_jj(envir.getWave(jWave));
					if (wave_jj.amp() == 0) continue;

					cx_rowvec::fixed<6> aux = hydroForce_2ndPot_coefs(wave_ii, wave_jj, envir.watDensity(), envir.watDepth(), envir.gravity()).st();
					if (iWave != jWave)
					{
						aux *= 2; // Because we are summing only the upper part of the matrix
					}
					amp_hydroForce_2nd.row(iWave - jWave) += aux;
				}
			}

			m_hydroForce_2nd_Array = nWaves * mkl_ifft_real(amp_hydroForce_2nd) % repmat(envir.getRampArray(), 1, 6);
		}
		else
		{
			for (unsigned int it = 0; it < t.size(); ++it)
			{
				for (int iWave = 0; iWave < nWaves; ++iWave)
				{
					const Wave &wave_ii(envir.getWave(iWave));
					if (wave_ii.amp() == 0) continue;

					for (int jWave = 0; jWave <= iWave; ++jWave)
					{
						const Wave &wave_jj(envir.getWave(jWave));
						if (wave_jj.amp() == 0) continue;

						double w_ii{ wave_ii.angFreq() }, w_jj{ wave_jj.angFreq() };
						cx_double sinCos{ cos((w_ii - w_jj) * t.at(it)), sin((w_ii - w_jj) * t.at(it)) };
						cx_rowvec::fixed<6> amp_dw = hydroForce_2ndPot_coefs(wave_ii, wave_jj, envir.watDensity(), envir.watDepth(), envir.gravity()).st();
						if (iWave != jWave)
						{
							amp_dw *= 2;
						}

						m_hydroForce_2nd_Array.row(it) += real(amp_dw * sinCos);
					}
				}
			}
			m_hydroForce_2nd_Array %= repmat(envir.getRampArray(), 1, 6);
		}
	}

}

/*****************************************************
	Forces acting on the Morison Element
*****************************************************/
// Make the local base of the cylinder at t = 0
void MorisonCirc::make_local_base(arma::vec::fixed<3> &xvec, arma::vec::fixed<3> &yvec, arma::vec::fixed<3> &zvec, const arma::vec::fixed<3> &n1, const arma::vec::fixed<3> &n2) const
{
	xvec.zeros();
	yvec.zeros();
	zvec = (n2 - n1) / arma::norm(n2 - n1, 2);

	// if Zlocal == Zglobal, REFlocal = REFglobal
	arma::vec::fixed<3> z_global = { 0,0,1 };
	if (arma::approx_equal(zvec, z_global, "absdiff", arma::datum::eps))
	{
		xvec = { 1, 0, 0 };
		yvec = { 0, 1, 0 };
	}
	else
	{
		yvec = arma::cross(z_global, zvec);
		yvec = yvec / arma::norm(yvec, 2);

		xvec = arma::cross(yvec, zvec);
		xvec = xvec / arma::norm(xvec, 2);
	}
}


vec::fixed<6> MorisonCirc::hydrostaticForce(const double rho, const double g) const
{
	// Forces and moments acting at the Morison Element
	vec::fixed<6> force(fill::zeros);

	// Use a more friendly notation
	double D = m_diam;

	vec::fixed<3> n1 = node1Pos();
	vec::fixed<3> n2 = node2Pos();
	vec::fixed<3> xvec(m_xvec), yvec(m_yvec), zvec(m_zvec);


	// If the cylinder is above the waterline, then the hydrostatic force is zero
	if (n1[2] >= 0)
	{
		return force;
	}

	// Calculation of the inclination of the cylinder (with respect to the
	// vertical), which is used in the calculation of the center of buoyoancy
	double alpha = std::acos(arma::dot(zvec, arma::vec::fixed<3> {0, 0, 1})); // zvec and {0, 0, 1} are both unit vectors
	double tanAlpha{ 0 };

	// Check if the angle is 90 degrees
	if (std::abs(alpha - arma::datum::pi / 2) > datum::eps)
	{
		tanAlpha = tan(alpha);
	}
	else
	{
		tanAlpha = arma::datum::inf;
	}

	// Length of the cylinder
	double L{ 0 };

	double xb{ 0 };
	double yb{ 0 };
	double zb{ 0 };

	// If the cylinder is completely submerged, then the center of volume is at the center of the cylinder
	if (n2[2] < 0)
	{
		L = norm(n2 - n1);
		xb = 0;
		yb = 0;
		zb = L / 2;
	}

	else if (is_finite(tanAlpha)) // otherwise, if the cylinder is not horizontal, the formulas for an inclined cylinder are used
	{
		// If only one of the nodes is below the water line, the coordinates of the other node
		// are changed by those of the intersection between the cylinder axis and the static
		// water line (defined by z_global = 0)
		n2 = n1 + (std::abs(0 - n1[2]) / (n2[2] - n1[2])) * norm(n2 - n1) * zvec;
		L = norm(n2 - n1);

		xb = tanAlpha * pow(D / 2, 2) / (4 * L);
		yb = 0;
		zb = (pow(tanAlpha*D / 2, 2) + 4 * pow(L, 2)) / (8 * L);
	}

	else // if the cylinder is horizontal and not completely submerged, forces and moments are equal to zero
	{
		return force;
	}

	// Vector Xb - n1(i.e., vector between the center of buoyancy and the
	// bottom node of the cylinder) written in the global coordinate frame
	vec::fixed<3> Xb_global = xb * xvec + yb * yvec + zb * zvec;

	// Volume of fluid displaced by the cylinder
	double Vol = arma::datum::pi * pow(D / 2, 2) * L;

	// Calculation of hydrostatic force and moment
	force[2] = rho * g * Vol; // Fx = Fy = 0 and Fz = Buoyancy force
	force.rows(3, 5) = cross(Xb_global, force.rows(0, 2));

	// The moment was calculated with relation to n1, which may be different from node1.
	// We need to change the fulcrum to node1
	force.rows(3, 5) = force.rows(3, 5) + cross(n1 - node1Pos(), force.rows(0, 2));

	return force;
}


// The vectors passed as reference are used to return the different components of the hydrodynamic force acting on the cylinder,
// without the need of calling three different methods for each component of the hydrodynamic force
vec::fixed<6> MorisonCirc::hydrodynamicForce(const ENVIR &envir, const int hydroMode, const vec::fixed<3> &refPt, const vec::fixed<3> &refPt_sd,
	vec::fixed<6> &force_drag, vec::fixed<6> &force_1, vec::fixed<6> &force_2,
	vec::fixed<6> &force_3, vec::fixed<6> &force_4, vec::fixed<6> &force_eta, vec::fixed<6> &force_rem) const
{
	// Forces and moments acting at the Morison Element
	vec::fixed<6> force(fill::zeros);

	// Make sure that the force components that are passed as reference are set to zero
	force_drag.zeros();
	force_1.zeros();
	force_2.zeros();
	force_3.zeros();
	force_4.zeros();
	force_eta.zeros();
	force_rem.zeros();

	// Use a more friendly notation
	double D = m_diam;
	double Cd = m_CD;
	double Cm = m_CM;
	double CdV_1 = m_axialCD_1;
	double CaV_1 = m_axialCa_1;
	double CdV_2 = m_axialCD_2;
	double CaV_2 = m_axialCa_2;
	double rho = envir.watDensity();

	// Nodes position and vectors of the local coordinate system
	vec::fixed<3> n1 = node1Pos_sd();
	vec::fixed<3> n2 = node2Pos_sd();

	// Same thing, but considering only the mean and slow drift displacements of the FOWT.
	// These ones are used to evaluate forces due to second order quantities (second order potential,
	// quadratic drag, etc).
	vec::fixed<3> n1_sd = node1Pos_sd();
	vec::fixed<3> n2_sd = node2Pos_sd();
	vec::fixed<3> xvec_sd = m_xvec_sd;
	vec::fixed<3> yvec_sd = m_yvec_sd;
	vec::fixed<3> zvec_sd = m_zvec_sd;

	// Velocity and acceleration of the cylinder nodes
	vec::fixed<3> v1 = node1Vel();
	vec::fixed<3> v2 = node2Vel();
	vec::fixed<3> a1 = node1AccCentrip();
	vec::fixed<3> a2 = node2AccCentrip();

	// TODO: this should be the difference between m_RotatMatrix and the one due to the mean body position Since this should not be used
	// when the filter is active, could use m_RotatMatrix - m_RotatMatrix_sd
	mat::fixed<6, 6> R(fill::eye);
	R *= -1;
	R.rows(0, 2).cols(0, 2) += m_RotatMatrix;
	R.rows(3, 5).cols(3, 5) += m_RotatMatrix;

	double L = arma::norm(n2 - n1, 2); // Total cylinder length
	double eta = 0; // Wave elevation above each integration node. Useful for Wheeler stretching method.
	double zwl = 0;
	if (envir.waveStret() == 2 && m_intersectWL.is_finite())
	{
		zwl = m_intersectWL.at(2);
	}

	// Initialization of some variables
	vec::fixed<3> n_ii(fill::zeros); // Coordinates of the integration point
	vec::fixed<3> n_ii_sd(fill::zeros); // Coordinates of the integration point - considering the body fixed at the initial position
	vec::fixed<3> vel_ii(fill::zeros); // Velocity of the integration point
	vec::fixed<3> acc_ii(fill::zeros); // Acceleration of the integration point - Only centripetal part

	vec::fixed<3> u1(fill::zeros); // Fluid velocity at the integration point
	vec::fixed<3> du1dt(fill::zeros); // Components of fluid acceleration at the integration point
	vec::fixed<3> du1dx(fill::zeros);
	vec::fixed<3> du1dy(fill::zeros);
	vec::fixed<3> du1dz(fill::zeros);
	vec::fixed<3> du2dt(fill::zeros);
	vec::fixed<3> a_c(fill::zeros); // Convective acceleration of the fluid
	vec::fixed<3> a_a(fill::zeros); // Axial-divergence acceleration of the fluid
	vec::fixed<3> a_r(fill::zeros); // Fluid acceleration associated with body rotation

	// Forces acting at the integration point and moment (with relation to n1) due to the force acting at the integration point
	// TODO: quase certeza que todas essas variaveis moment sao desnecessarias
	vec::fixed<3> force_drag_ii(fill::zeros); // Drag component
	vec::fixed<3> force_1_ii(fill::zeros); // First part - Forces due to 1st order potential
	vec::fixed<3> force_2_ii(fill::zeros); // Second part - Forces due to 2nd order potential
	vec::fixed<3> force_3_ii(fill::zeros); // Third part - Forces due to the convective acceleration
	vec::fixed<3> force_4_ii(fill::zeros); // Fourth part - Forces due to the axial-divergence acceleration
	vec::fixed<3> force_rem_ii(fill::zeros); // Remaining components

	// Relative distance between the integration point and the bottom node
	double lambda{ 0 };

	// Component of the velocity and acceleration that is parallel to the axis of the cylinder
	vec::fixed<3> v_axial = arma::dot(v1, zvec_sd) * zvec_sd; // Velocities are included in second order terms only, hence are calculate at the sd position

	// Useful auxilliary variables to avoid recalculating things
	vec::fixed<3> aux_force(fill::zeros);
	double aux{ 0 };

	/*=================================
		Forces along the length of the cylinder - Considering the instantaneous position
	==================================*/

	// Force due to first-order acceleration - part along the length of the cylinder is integrated analytically
	vec::fixed<6> auxForce = hydroForce_1st(envir, hydroMode);
	force_1 += auxForce;
	force_1.rows(3, 5) += cross(n1 - refPt, auxForce.rows(0, 2));

	// Second order forces
	if (hydroMode == 2)
	{
		force_1 += R * auxForce;
		force_1.rows(3, 5) += cross(n1 - refPt, R.rows(0, 2).cols(0, 2) * auxForce.rows(0, 2));

		auxForce = hydroForce_2ndPot(envir);
		force_2 += auxForce;
		force_2.rows(3, 5) += cross(n1 - refPt, auxForce.rows(0, 2));

		auxForce = hydroForce_convecAcc(envir);
		force_3 += auxForce;
		force_3.rows(3, 5) += cross(n1 - refPt, auxForce.rows(0, 2));
	}

	auxForce = hydroForce_drag(envir);
	force_drag += auxForce;
	force_drag.rows(3, 5) += cross(n1 - refPt, auxForce.rows(0, 2));

	/* UNCOMMENT LATER
	// Drag forces	


	double Lw = L;
	int ncyl = m_numIntPoints;
	if (n2.at(2) > zwl)
	{
		Lw = L * (zwl - n1.at(2)) / (n2.at(2) - n1.at(2));
		ncyl = static_cast<int>(std::ceil(Lw / L * (m_numIntPoints - 1))) + 1; // Number of points to discretize the part of the cylinder that is below the water		
	}
	if (ncyl % 2 == 0)
	{
		ncyl += 1;
	}
	double dL = Lw / (ncyl - 1); // length of each interval between points

	aux = datum::pi * D*D / 4. * rho;
	if (hydroMode > 1)
	{
		for (int ii = 1; ii <= ncyl; ++ii)
		{
			n_ii = n1 + dL * (ii - 1) * zvec_sd; // Coordinates of the integration point

			double xii = m_node1Pos.at(0) + dL * (ii - 1) * m_zvec.at(0) - n_ii.at(0);
			double yii = m_node1Pos.at(1) + dL * (ii - 1) * m_zvec.at(1) - n_ii.at(1);
			double zii = m_node1Pos.at(2) + dL * (ii - 1) * m_zvec.at(2) - n_ii.at(2);

			if (n_ii[2] >= zwl && ii == ncyl)
			{
				n_ii[2] = zwl;
			}

			if (envir.waveStret() == 2 && hydroMode == 2)
			{
				eta = envir.waveElev(n_ii.at(0), n_ii.at(1));
			}

			// Component of the fluid acceleration at the integration point that is perpendicular to the axis of the cylinder.
			vec::fixed<3> dadx = envir.da1dx(n_ii, eta);
			vec::fixed<3> dady = envir.da1dy(n_ii, eta);
			vec::fixed<3> dadz = envir.da1dz(n_ii, eta);
			du1dt = envir.du1dt(n_ii, eta);
			du1dt = arma::dot(du1dt, R.rows(0, 2).cols(0, 2) * xvec_sd) * xvec_sd + arma::dot(du1dt, R.rows(0, 2).cols(0, 2) * yvec_sd) * yvec_sd;
			du1dt += arma::dot(dadx * xii, xvec_sd) * xvec_sd + arma::dot(dadx * xii, yvec_sd) * yvec_sd
				+ arma::dot(dady * yii, xvec_sd) * xvec_sd + arma::dot(dady * yii, yvec_sd) * yvec_sd
				+ arma::dot(dadz * zii, xvec_sd) * xvec_sd + arma::dot(dadz * zii, yvec_sd) * yvec_sd;

			force_1_ii = aux * Cm * du1dt;

			// Component of the acceleration of the integration point
			// that is perpendicular to the axis of the cylinder
			lambda = norm(n_ii - n1, 2) / L;
			acc_ii = a1 + lambda * (a2 - a1);
			acc_ii = arma::dot(acc_ii, xvec_sd)*xvec_sd + arma::dot(acc_ii, yvec_sd)*yvec_sd;

			force_rem_ii = -aux * (Cm - 1) * acc_ii;

			// Integrate the forces along the cylinder using Simpson's Rule
			if (ii == 1 || ii == ncyl)
			{
				force_1 += (dL / 3.0) * join_cols(force_1_ii, cross(n_ii - refPt, force_1_ii));
				force_rem += (dL / 3.0) * join_cols(force_rem_ii, cross(n_ii - refPt, force_rem_ii));
			}
			else if (ii % 2 == 0)
			{
				force_1 += (4 * dL / 3.0) * join_cols(force_1_ii, cross(n_ii - refPt, force_1_ii));
				force_rem += (4 * dL / 3.0) * join_cols(force_rem_ii, cross(n_ii - refPt, force_rem_ii));
			}
			else
			{
				force_1 += (2 * dL / 3.0) * join_cols(force_1_ii, cross(n_ii - refPt, force_1_ii));
				force_rem += (2 * dL / 3.0) * join_cols(force_rem_ii, cross(n_ii - refPt, force_rem_ii));
			}
		}
	}

	/*=================================
		Forces along the length of the cylinder - Considering the slow (or fixed) position
	==================================*/
	/*Lw = L;
	ncyl = m_numIntPoints;
	if (n2_sd.at(2) > 0)
	{
		Lw = L * (0 - n1_sd.at(2)) / (n2_sd.at(2) - n1_sd.at(2));
		ncyl = static_cast<int>(std::ceil(Lw / L * (m_numIntPoints - 1))) + 1; // Number of points to discretize the part of the cylinder that is below the water		
	}
	if (ncyl % 2 == 0)
	{
		ncyl += 1;
	}
	dL = Lw / (ncyl - 1); // length of each interval between points

	force_rem_ii.zeros();
	for (int ii = 1; ii <= ncyl; ++ii)
	{
		n_ii_sd = n1_sd + dL * (ii - 1) * zvec_sd;

		if (n_ii_sd[2] >= 0 && ii == ncyl)
		{
			n_ii_sd[2] = 0;
		}

		if (envir.waveStret() == 2 && hydroMode == 2)
		{
			eta = envir.waveElev(n_ii_sd.at(0), n_ii_sd.at(1));
		}

		// Velocity of the integration point
		lambda = norm(n_ii_sd - n1_sd, 2) / L;
		vel_ii = v1 + lambda * (v2 - v1);

		// If required, calculate the other second-order forces,
		if (hydroMode == 2)
		{
			// Fluid velocity at the integration point.		
			u1 = envir.u1(n_ii_sd, eta);
			u1 -= arma::dot(u1, zvec_sd) * zvec_sd;

			// 3rd component: Force due to convective acceleration
			u1 = envir.u1(n_ii_sd, eta);
			du1dx = envir.du1dx(n_ii_sd, eta);
			du1dy = envir.du1dy(n_ii_sd, eta);
			du1dz = envir.du1dz(n_ii_sd, eta);

			a_c.at(0) = u1.at(0) * du1dx.at(0) + u1.at(1) * du1dy.at(0) + u1.at(2) * du1dz.at(0);
			a_c.at(1) = u1.at(0) * du1dx.at(1) + u1.at(1) * du1dy.at(1) + u1.at(2) * du1dz.at(1);
			a_c.at(2) = u1.at(0) * du1dx.at(2) + u1.at(1) * du1dy.at(2) + u1.at(2) * du1dz.at(2);
			a_c -= arma::dot(a_c, zvec_sd) * zvec_sd;

			force_3_ii = aux * Cm * a_c;

			// 4th component: Force due to axial-divergence acceleration
			double dwdz = arma::dot(du1dx, zvec_sd) * zvec_sd.at(0) + arma::dot(du1dy, zvec_sd) * zvec_sd.at(1) + arma::dot(du1dz, zvec_sd) * zvec_sd.at(2);
			a_a = dwdz * (arma::dot(u1 - vel_ii, xvec_sd)*xvec_sd + arma::dot(u1 - vel_ii, yvec_sd)*yvec_sd);
			force_4_ii = aux * (Cm - 1) * a_a;

			// Add to remaining forces: Force due to cylinder rotation				
			a_r = 2 * arma::dot(u1 - vel_ii, zvec_sd) * (1 / L) * (arma::dot(v2 - v1, xvec_sd) * xvec_sd + arma::dot(v2 - v1, yvec_sd) * yvec_sd);
			force_rem_ii = -aux * (Cm - 1) * a_r;
		}

		// Integrate the forces along the cylinder using Simpson's Rule
		if (ii == 1 || ii == ncyl)
		{
			force_3 += (dL / 3.0) * join_cols(force_3_ii, cross(n_ii_sd - refPt_sd, force_3_ii));
			force_4 += (dL / 3.0) * join_cols(force_4_ii, cross(n_ii_sd - refPt_sd, force_4_ii));
			force_rem += (dL / 3.0) * join_cols(force_rem_ii, cross(n_ii_sd - refPt_sd, force_rem_ii));
		}
		else if (ii % 2 == 0)
		{
			force_3 += (4 * dL / 3.0) * join_cols(force_3_ii, cross(n_ii_sd - refPt_sd, force_3_ii));
			force_4 += (4 * dL / 3.0) * join_cols(force_4_ii, cross(n_ii_sd - refPt_sd, force_4_ii));
			force_rem += (4 * dL / 3.0) * join_cols(force_rem_ii, cross(n_ii_sd - refPt_sd, force_rem_ii));
		}
		else
		{
			force_3 += (2 * dL / 3.0) * join_cols(force_3_ii, cross(n_ii_sd - refPt_sd, force_3_ii));
			force_4 += (2 * dL / 3.0) * join_cols(force_4_ii, cross(n_ii_sd - refPt_sd, force_4_ii));
			force_rem += (2 * dL / 3.0) * join_cols(force_rem_ii, cross(n_ii_sd - refPt_sd, force_rem_ii));
		}
	}

	// Eta: Force due to the wave elevation.
	// It is computed here only if Taylor series for wave stretching was chosen and
	// if the cylinder intersects the water line.
	// If waveStret == 0, this effect is not included in the analysis, 
	// and if waveStret > 1, this force component is included in force_1.
	if (hydroMode == 2 && envir.waveStret() == 1 && (n2.at(2)*n1.at(2) < 0))
	{
		n_ii = (n2 - n1) * (0 - n1.at(2)) / (n2.at(2) - n1.at(2)) + n1; // Coordinates of the intersection with the still water line;				
		n_ii.at(2) = 0; // Since envir.du1dt returns 0 for z > 0, this line is necessary to make sure that the z coordinate of n_ii is exactly 0, and not slightly above due to roundoff errors.
		du1dt = envir.du1dt(n_ii, 0);
		du1dt = arma::dot(du1dt, xvec_sd) * xvec_sd + arma::dot(du1dt, yvec_sd) * yvec_sd;
		eta = envir.waveElev(n_ii.at(0), n_ii.at(1));
		force_eta.rows(0, 2) = (datum::pi * D*D / 4.) * rho * Cm * du1dt * (eta - m_Zwl);
		force_eta.rows(3, 5) = cross(n_ii - refPt, force_eta.rows(0, 2));
	}


	/*=================================
		Forces at the extremities of the cylinder
	==================================*/
	/*// Calculate the force acting on the bottom of the cylinder
	if (n1.at(2) <= zwl)
	{
		// Kinematics
		a_c.zeros();
		du2dt.zeros();
		if (hydroMode == 2)
		{
			du1dx = envir.du1dx(n1_sd, 0);
			du1dy = envir.du1dy(n1_sd, 0);
			du1dz = envir.du1dz(n1_sd, 0);

			a_c.at(0) = u1.at(0) * du1dx.at(0) + u1.at(1) * du1dy.at(0) + u1.at(2) * du1dz.at(0);
			a_c.at(1) = u1.at(0) * du1dx.at(1) + u1.at(1) * du1dy.at(1) + u1.at(2) * du1dz.at(1);
			a_c.at(2) = u1.at(0) * du1dx.at(2) + u1.at(1) * du1dy.at(2) + u1.at(2) * du1dz.at(2);
			a_c = arma::dot(a_c, zvec_sd) * zvec_sd;
		}

		// Inertial force - pt3
		aux_force = aux * a_c;
		force_3.rows(0, 2) += aux_force;
		force_3.rows(3, 5) += cross(n1_sd - refPt_sd, aux_force);

		// Remaining force components
		aux_force = -aux * arma::dot(a1, zvec_sd) * zvec_sd;
		force_rem.rows(0, 2) += aux_force;
		force_rem.rows(3, 5) += cross(n1 - refPt, aux_force);

		if (m_botPressFlag && hydroMode == 2)
		{
			aux = datum::pi * 0.25 * D*D;

			// Inertial force - pt3 - Quadratic pressure drop from Rainey's formulation
			aux_force = -0.5 * aux * rho * (Cm - 1) * (pow(arma::dot(envir.u1(n1_sd, 0) - v1, xvec_sd), 2) + pow(arma::dot(envir.u1(n1_sd, 0) - v1, yvec_sd), 2)) * zvec_sd;
			force_3.rows(0, 2) += aux_force;
			force_3.rows(3, 5) += cross(n1_sd - refPt_sd, aux_force);

			// Inertial force - Remaining - Point load from Rainey's formulation that results in a Munk moment
			aux_force = aux * (Cm - 1) * cdot(u1 - v_axial, zvec_sd) * ((envir.u1(n1_sd, 0) - u1) - (v1 - v_axial));
			force_rem.rows(0, 2) += aux_force;
			force_rem.rows(3, 5) += cross(n1_sd - refPt_sd, aux_force);
		}
	}

	// Calculate the force acting on the top of the cylinder
	if (n2.at(2) <= zwl)
	{
		// Kinematics
		a_c.zeros();
		du2dt.zeros();
		if (hydroMode == 2)
		{
			du1dx = envir.du1dx(n2_sd, 0);
			du1dy = envir.du1dy(n2_sd, 0);
			du1dz = envir.du1dz(n2_sd, 0);

			a_c.at(0) = u1.at(0) * du1dx.at(0) + u1.at(1) * du1dy.at(0) + u1.at(2) * du1dz.at(0);
			a_c.at(1) = u1.at(0) * du1dx.at(1) + u1.at(1) * du1dy.at(1) + u1.at(2) * du1dz.at(1);
			a_c.at(2) = u1.at(0) * du1dx.at(2) + u1.at(1) * du1dy.at(2) + u1.at(2) * du1dz.at(2);
			a_c = arma::dot(a_c, zvec_sd) * zvec_sd;
		}

		// Inertial force - pt3
		aux_force = aux * a_c;
		force_3.rows(0, 2) += aux_force;
		force_3.rows(3, 5) += cross(n2_sd - refPt_sd, aux_force);

		// Remaining force components
		aux_force = -aux * arma::dot(a2, zvec_sd) * zvec_sd;
		force_rem.rows(0, 2) += aux_force;
		force_rem.rows(3, 5) += cross(n2 - refPt, aux_force);

		if (m_botPressFlag && hydroMode == 2)
		{
			aux = datum::pi * 0.25 * D*D;

			// Inertial force - pt3 - Quadratic pressure drop from Rainey's formulation
			aux_force = 0.5 * aux * rho * (Cm - 1) * pow(norm((envir.u1(n2_sd, 0) - u1) - (v1 - v_axial)), 2) * zvec_sd;
			force_3.rows(0, 2) += aux_force;
			force_3.rows(3, 5) += cross(n2_sd - refPt_sd, aux_force);

			// Inertial force - Remaining - Point load from Rainey's formulation that results in a Munk moment
			aux_force = -aux * (Cm - 1) * cdot(u1 - v_axial, zvec_sd) * ((envir.u1(n2_sd, 0) - u1) - (v1 - v_axial));
			force_rem.rows(0, 2) += aux_force;
			force_rem.rows(3, 5) += cross(n2_sd - refPt_sd, aux_force);
		}
	}
	*/
	/*
		Total force
	*/
	force = force_drag + force_1 + force_2 + force_3 + force_4 + force_eta + force_rem;

	return force;
}

/* Functions to evaluate force components 
They are the ones that choose between already evaluated quantities, when availabel, or calculating the forces at each time step
*/

// TODO: Implement Wheeler stretching in force components
vec::fixed<6> MorisonCirc::hydroForce_1st(const ENVIR &envir, const int hydroMode) const
{
	vec::fixed<6> force(fill::zeros);

	if (m_hydroForce_1st_Array.is_empty())
	{
		for (unsigned int ii = 0; ii < envir.numberOfWaveComponents(); ++ii)
		{
			const Wave &wave(envir.getWave(ii));
			double w{ wave.angFreq() };
			cx_double sinCos({ cos(w * envir.time()), sin(w * envir.time()) });

			force += real(hydroForce_1st_coefs(wave, envir.watDensity(), envir.watDepth(), envir.gravity()) * sinCos) * envir.ramp();
		}
	}
	else
	{
		uword ind1 = envir.getInd4interp1();
		force = m_hydroForce_1st_Array.row(ind1).t();

		if (envir.shouldInterp())
		{
			uword ind2 = envir.getInd4interp2();
			const vec &t = envir.getTimeArray();
			force += (m_hydroForce_1st_Array.row(ind2).t() - m_hydroForce_1st_Array.row(ind1).t()) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
		}
	}

	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_drag(const ENVIR &envir) const
{
	vec::fixed<6> force;
	if (m_node1Pos_sd.at(2) > 0)
		return force;

	if (m_u1_Array_x.is_empty())
	{
		force = hydroForce_drag_calculate(envir);
	}
	else
	{
		force = hydroForce_drag_already_calculated(envir);
	}
	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_2ndPot(const ENVIR &envir) const
{
	vec::fixed<6> force(fill::zeros);

	if (m_hydroForce_2nd_Array.is_empty())
	{
		// When i == j, du2dt = {0,0,0}, so it is safe to skip this part of the loop.
		// Besides, as only the real part of the second-order difference-frequency potential is used,
		// the acceleration due to a pair ij is equal to ji.
		for (unsigned int ii = 0; ii < envir.numberOfWaveComponents(); ++ii)
		{
			for (unsigned int jj = ii + 1; jj < envir.numberOfWaveComponents(); ++jj)
			{
				const Wave &wave_ii(envir.getWave(ii));
				const Wave &wave_jj(envir.getWave(jj));

				double w_ii{ wave_ii.angFreq() }, w_jj{ wave_jj.angFreq() };
				cx_double sinCos({ cos((w_ii - w_jj) * envir.time()), sin((w_ii - w_jj) * envir.time()) });

				force += 2 * real(hydroForce_2ndPot_coefs(wave_ii, wave_jj, envir.watDensity(), envir.watDepth(), envir.gravity()) * sinCos) * envir.ramp();
			}
		}
	}
	else
	{
		uword ind1 = envir.getInd4interp1();
		force = m_hydroForce_2nd_Array.row(ind1).t();

		if (envir.shouldInterp())
		{
			uword ind2 = envir.getInd4interp2();
			const vec &t = envir.getTimeArray();
			force += (m_hydroForce_2nd_Array.row(ind2).t() - m_hydroForce_2nd_Array.row(ind1).t()) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
		}
	}

	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_convecAcc(const ENVIR &envir) const
{
	vec::fixed<6> force;
	if (m_node1Pos_sd.at(2) > 0)
		return force;

	if (m_du1dx_Array_x.is_empty())
	{
		force = hydroForce_convecAcc_calculate(envir);
	}
	else
	{
		force = hydroForce_convecAcc_already_calculated(envir);
	}
	return force;
}

/*
Below are the functions with the force expressions
*/
cx_vec::fixed<6> MorisonCirc::hydroForce_1st_coefs(const Wave &wave, double watDensity, double watDepth, double gravity) const
{
	double rho = watDensity;
	double h = watDepth;
	double g = gravity;
	double R = m_diam / 2.;
	double pi = arma::datum::pi;
	double k{ wave.waveNumber() };

	if (k <= 0)
	{
		return fill::zeros;
	}

	vec::fixed<3> Zvec{ 0, 0, 1 }; // Vectors of the global basis
	vec::fixed<3> Xvec{ 1, 0, 0 };

	vec::fixed<3> n1{ node1Pos_sd() }, n2{ node2Pos_sd() };
	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };
	if (!m_flagFixed)
	{
		n1 = node1Pos(); n2 = node2Pos(); xvec = m_xvec; yvec = m_yvec; zvec = m_zvec;
	}

	if (n1.at(2) >= 0)
	{
		return fill::zeros;
	}

	bool evaluateTopNode{ true };
	if (n2.at(2) > 0)
	{
		n2 = n1 + (abs(0 - n1(2)) / (n2(2) - n1(2))) * arma::norm(n2 - n1) * zvec;
		evaluateTopNode = false;
	}

	double x1(n1[0]), y1(n1[1]), z1(n1[2]);
	double x2(n2[0]), y2(n2[1]), z2(n2[2]);
	double L = norm(n2 - n1);

	// Calculation of the inclination of the cylinder with respect to the vertical
	double alpha = std::acos(arma::dot(zvec, arma::vec::fixed<3> {0, 0, 1})); // zvec and {0, 0, 1} are both unit vectors
	double tanAlpha{ 0 }, cosAlpha{ 0 }, sinAlpha{ 0 };

	// Check if the angle is 90 degrees
	if (std::abs(alpha - arma::datum::pi / 2) > datum::eps)
	{
		tanAlpha = tan(alpha);
		cosAlpha = cos(alpha);
		sinAlpha = sin(alpha);
	}
	else
	{
		tanAlpha = arma::datum::inf;
		sinAlpha = 1;
	}

	// Calculate the angle that the cylinder makes with the global x axis
	// Different method than the one used for tanAlpha because in here we need the sign of the angle
	double psi{ 0 };
	if (std::abs(alpha) > datum::eps)
	{
		vec::fixed<3> zproj = zvec - dot(zvec, Zvec)*Zvec;
		zproj = zproj / norm(zproj);

		psi = -atan2(dot(cross(zproj, Xvec), Zvec), dot(zproj, Xvec));
	}

	double w{ wave.angFreq() }, A{ wave.amp() };
	double cosBeta{ wave.cosBeta() }, sinBeta{ wave.sinBeta() };
	double beta = wave.direction() * pi / 180.;
	double phase = wave.phase() * pi / 180.;
	double cosP{ cos(phase) }, sinP{ sin(phase) };

	// Avoid recalculting cossine and sine functions
	double cosBetaPsi = cos(beta - psi);
	double sinBetaPsi = sin(beta - psi);

	// Auxiliar variables to make the expressions shorter
	double mult_h_cos(0), mult_h_sin(0), mult_v_cos(0), mult_v_sin(0);
	double p1 = k * cosBeta*x1 + k * sinBeta*y1 + phase;
	double p2 = k * cosBeta*x2 + k * sinBeta*y2 + phase;
	double I = k * cosBetaPsi * tanAlpha;
	double Ih = (k * cosBeta*(x2 - x1) + k * sinBeta*(y2 - y1)) / L;
	double Q{ m_CM * w * w * A }, Qz1{ m_axialCa_1 * w * w * A };
	double Qz2 = (evaluateTopNode ? m_axialCa_2 * w * w * A : 0);


	// Ratios of hyperbolic functions need a special treatment otherwise they yield inf in large water depth
	double cosh_sinh_1{ 0 }, cosh_sinh_2{ 0 }, cosh_cosh_1{ 0 }, cosh_cosh_2{ 0 }, sinh_sinh_1{ 0 }, sinh_sinh_2{ 0 };
	if (k*h >= 10)
	{
		cosh_sinh_1 = exp(k*z1);
		cosh_cosh_1 = cosh_sinh_1;
		sinh_sinh_1 = cosh_sinh_1;

		cosh_sinh_2 = exp(k*z2);
		cosh_cosh_2 = cosh_sinh_2;
		sinh_sinh_2 = cosh_sinh_2;
	}
	else
	{
		cosh_sinh_1 = cosh(k * (z1 + h)) / sinh(k*h);
		cosh_cosh_1 = cosh(k * (z1 + h)) / cosh(k*h);
		sinh_sinh_1 = sinh(k * (z1 + h)) / sinh(k*h);

		cosh_sinh_2 = cosh(k * (z2 + h)) / sinh(k*h);
		cosh_cosh_2 = cosh(k * (z2 + h)) / cosh(k*h);
		sinh_sinh_2 = sinh(k * (z2 + h)) / sinh(k*h);
	}

	/*
		FORCES
	*/
	// The integration is different for a horizontal cylinder, as the integration variable
	// along the cylinder is not related to the vertical position. This special case is treated separately.
	if (arma::is_finite(tanAlpha))
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = k * sin(p2) * sinh_sinh_2 - I * cos(p2) * cosh_sinh_2;
		mult_h_cos += -k * sin(p1) * sinh_sinh_1 + I * cos(p1) * cosh_sinh_1;
		mult_h_cos *= 1 / (I*I + k * k);

		mult_h_sin = k * cos(p2) * sinh_sinh_2 + I * sin(p2) * cosh_sinh_2;
		mult_h_sin += -k * cos(p1) * sinh_sinh_1 - I * sin(p1) * cosh_sinh_1;
		mult_h_sin *= 1 / (I*I + k * k);

		// This cosAlpha is due to the change of integration variable.
		// We know cosAlpha !=0 because horizontal cylinders are treated separately
		mult_h_cos *= 1 / cosAlpha;
		mult_h_sin *= 1 / cosAlpha;

		// Contribution of the vertical acceleration
		mult_v_cos = k * cos(p2) * cosh_sinh_2 + I * sin(p2) * sinh_sinh_2;
		mult_v_cos += -k * cos(p1) * cosh_sinh_1 - I * sin(p1) * sinh_sinh_1;
		mult_v_cos *= 1 / (I*I + k * k);

		mult_v_sin = k * sin(p2) * cosh_sinh_2 - I * cos(p2) * sinh_sinh_2;
		mult_v_sin += -k * sin(p1) * cosh_sinh_1 + I * cos(p1) * sinh_sinh_1;
		mult_v_sin *= 1 / (I*I + k * k);

		mult_v_cos *= 1 / cosAlpha;
		mult_v_sin *= -1 / cosAlpha;
	}
	else
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = -cos(p2) + cos(p1);
		mult_h_cos *= 1 / Ih;

		mult_h_sin = sin(p2) - sin(p1);
		mult_h_sin *= 1 / Ih;

		// Contribution of the vertical acceleration
		mult_v_cos = mult_h_sin * sinh_sinh_1;
		mult_v_sin = -mult_h_cos * sinh_sinh_1;

		// Need the factor of the water depth in the horizontal acceleration as well
		mult_h_cos *= cosh_sinh_1;
		mult_h_sin *= cosh_sinh_1;
	}

	// Forces along the local x and y axes
	double fx_cos = Q * (cosBetaPsi * cosAlpha * mult_h_cos + sinAlpha * mult_v_cos);
	double fx_sin = Q * (cosBetaPsi * cosAlpha * mult_h_sin + sinAlpha * mult_v_sin);
	double fy_cos = Q * sinBetaPsi * mult_h_cos;
	double fy_sin = Q * sinBetaPsi * mult_h_sin;

	// Forces along the local z axis. Factor 4*R/3. due to this force being acceleration * volume, while in the end 
	// of this function things are multiplied by the area of the circle
	double fz_cos = (4 * R / 3.) * cosBetaPsi * sinAlpha * (Qz1 * cosh_sinh_1 * sin(p1) - Qz2 * cosh_sinh_2 * sin(p2)); // Contribution of horizontal acceleration
	double fz_sin = (4 * R / 3.) * cosBetaPsi * sinAlpha * (-Qz1 * cosh_sinh_1 * cos(p1) + Qz2 * cosh_sinh_2 * cos(p2));
	fz_cos += -(4 * R / 3.) * cosAlpha * (Qz1 * sinh_sinh_1 * cos(p1) - Qz2 * sinh_sinh_2 * cos(p2)); // Contribution of vertical acceleration
	fz_sin += (4 * R / 3.) * cosAlpha * (Qz1 * sinh_sinh_1 * sin(p1) - Qz2 * sinh_sinh_2 * sin(p2));

	/*
		MOMENTS
	*/
	if (arma::is_finite(tanAlpha))
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = k * sinh_sinh_2 * ((k*k + I * I)*(z2 - z1)*sin(p2) + 2 * I*cos(p2))
			+ cosh_sinh_2 * (-I * (k*k + I * I)*(z2 - z1)*cos(p2) + (I*I - k * k)*sin(p2));
		mult_h_cos += -k * sinh_sinh_1 * 2 * I*cos(p1)
			- cosh_sinh_1 * (I*I - k * k)*sin(p1);
		mult_h_cos *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_h_sin = k * sinh_sinh_2 * ((k*k + I * I)*(z2 - z1)*cos(p2) - 2 * I*sin(p2))
			+ cosh_sinh_2 * (I*(k*k + I * I)*(z2 - z1)*sin(p2) + (I*I - k * k)*cos(p2));
		mult_h_sin += k * sinh_sinh_1 * 2 * I*sin(p1)
			- cosh_sinh_1 * (I*I - k * k)*cos(p1);
		mult_h_sin *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_h_cos *= 1 / cosAlpha / cosAlpha;
		mult_h_sin *= 1 / cosAlpha / cosAlpha;

		// Contribution of the vertical acceleration
		mult_v_cos = k * cosh_sinh_2 * ((k*k + I * I)*(z2 - z1)*cos(p2) - 2 * I*sin(p2))
			+ sinh_sinh_2 * (I*(k*k + I * I)*(z2 - z1)*sin(p2) + (I*I - k * k)*cos(p2));
		mult_v_cos += k * cosh_sinh_1 * 2 * I*sin(p1)
			- sinh_sinh_1 * (I*I - k * k)*cos(p1);
		mult_v_cos *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_v_sin = k * cosh_sinh_2 * ((k*k + I * I)*(z2 - z1)*sin(p2) + 2 * I*cos(p2))
			+ sinh_sinh_2 * (-I * (k*k + I * I)*(z2 - z1)*cos(p2) + (I*I - k * k)*sin(p2));
		mult_v_sin += -k * cosh_sinh_1 * 2 * I*cos(p1)
			- sinh_sinh_1 * (I*I - k * k)*sin(p1);
		mult_v_sin *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_v_cos *= 1 / cosAlpha / cosAlpha;
		mult_v_sin *= -1 / cosAlpha / cosAlpha;
	}
	else
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = -Ih * L*cos(p2) + sin(p2) - sin(p1);
		mult_h_cos *= 1 / (Ih*Ih);

		mult_h_sin = Ih * L*sin(p2) + cos(p2) - cos(p1);
		mult_h_sin *= 1 / (Ih*Ih);;

		// Contribution of the vertical acceleration
		mult_v_cos = mult_h_sin * sinh_sinh_1;
		mult_v_sin = -mult_h_cos * sinh_sinh_1;

		// Need the factor of the water depth in the horizontal acceleration as well
		mult_h_cos *= cosh_sinh_1;
		mult_h_sin *= cosh_sinh_1;
	}

	// Forces along the local x and y axes
	double mx_cos = -Q * sinBetaPsi * mult_h_cos;
	double mx_sin = -Q * sinBetaPsi * mult_h_sin;
	double my_cos = Q * (cosBetaPsi * cosAlpha * mult_h_cos + sinAlpha * mult_v_cos);
	double my_sin = Q * (cosBetaPsi * cosAlpha * mult_h_sin + sinAlpha * mult_v_sin);

	if (m_botPressFlag)
	{
		fz_cos += g * A * cos(p1) * cosh_cosh_1;
		fz_sin += -g * A * sin(p1) * cosh_cosh_1;
		if (evaluateTopNode)
		{
			fz_cos -= g * A * cos(p2) * cosh_cosh_2;
			fz_sin -= -g * A * sin(p2) * cosh_cosh_2;
		}
	}

	cx_vec::fixed<6> coef(fill::zeros);
	for (int ii = 0; ii < 3; ++ii)
	{
		coef.at(ii) = cx_double{ fx_cos * xvec(ii) + fy_cos * yvec(ii) + fz_cos * zvec(ii) , fx_sin * xvec(ii) + fy_sin * yvec(ii) + fz_sin * zvec(ii) };
	}
	for (int ii = 3; ii < 6; ++ii)
	{
		coef.at(ii) = cx_double{ mx_cos * xvec(ii - 3) + my_cos * yvec(ii - 3) , mx_sin * xvec(ii - 3) + my_sin * yvec(ii - 3) };
	}

	// Moments have to be with respect to the slow n1 position, as this is assumed by the other functions
	if (!m_flagFixed)
	{
		cx_vec::fixed<3> aux = { cross(n1 - node1Pos_sd(), real(coef.rows(0,2))), cross(n1 - node1Pos_sd(), imag(coef.rows(0,2))) };
		coef.rows(3, 5) += aux;
	}


	return rho * pi* R * R * coef;
}

// TODO: this function has many similarities with its 1st order counterpart. It would be better to group 
// common code in an auxiliary function.
cx_vec::fixed<6> MorisonCirc::hydroForce_2ndPot_coefs(const Wave &wave_ii, const Wave &wave_jj , double watDensity, double watDepth, double gravity) const
{
	vec::fixed<6> force(fill::zeros);

	double rho = watDensity;
	double h = watDepth;
	double g = gravity;
	double R = m_diam / 2.;
	double pi = arma::datum::pi;
	vec::fixed<3> Zvec{ 0, 0, 1 }; // Vectors of the global basis
	vec::fixed<3> Xvec{ 1, 0, 0 };

	vec::fixed<3> n1{ node1Pos_sd() }, n2{ node2Pos_sd() };
	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };

	if (n1.at(2) >= 0)
	{
		return fill::zeros;
	}

	bool evaluateTopNode{ true };
	if (n2.at(2) > 0)
	{
		n2 = n1 + (abs(0 - n1(2)) / (n2(2) - n1(2))) * arma::norm(n2 - n1) * zvec;
		evaluateTopNode = false;
	}

	double x1(n1[0]), y1(n1[1]), z1(n1[2]);
	double x2(n2[0]), y2(n2[1]), z2(n2[2]);
	double L = norm(n2 - n1);

	// Calculation of the inclination of the cylinder with respect to the vertical
	double alpha = std::acos(arma::dot(zvec, arma::vec::fixed<3> {0, 0, 1})); // zvec and {0, 0, 1} are both unit vectors
	double tanAlpha{ 0 }, cosAlpha{ 0 }, sinAlpha{ 0 };

	// Check if the angle is 90 degrees
	if (std::abs(alpha - arma::datum::pi / 2) > datum::eps)
	{
		tanAlpha = tan(alpha);
		cosAlpha = cos(alpha);
		sinAlpha = sin(alpha);
	}
	else
	{
		tanAlpha = arma::datum::inf;
		sinAlpha = 1;
	}

	// Calculate the angle that the cylinder makes with the global x axis
	// Different method than the one used for tanAlpha because in here we need the sign of the angle
	double psi{ 0 };
	if (std::abs(alpha) > datum::eps)
	{
		vec::fixed<3> zproj = zvec - dot(zvec, Zvec)*Zvec;
		zproj = zproj / norm(zproj);

		psi = -atan2(dot(cross(zproj, Xvec), Zvec), dot(zproj, Xvec));
	}
	
	double w_ii{ wave_ii.angFreq() }, k_ii{ wave_ii.waveNumber() }, A_ii{ wave_ii.amp() };
	double w_jj{ wave_jj.angFreq() }, k_jj{ wave_jj.waveNumber() }, A_jj{ wave_jj.amp() };
	double cosBeta_ii{ wave_ii.cosBeta() }, sinBeta_ii{ wave_ii.sinBeta() };
	double cosBeta_jj{ wave_jj.cosBeta() }, sinBeta_jj{ wave_jj.sinBeta() };
	double beta_ii{ wave_ii.direction() * pi / 180. }, beta_jj{ wave_jj.direction() * pi / 180. };
	double phase_ii{ wave_ii.phase() * pi / 180. }, phase_jj{ wave_jj.phase() * pi / 180. };

	if (k_ii == 0 || k_jj == 0)
	{
		return fill::zeros;
	}

	// Second-order potential does not contribute to the mean force
	if (w_ii == w_jj)
	{
		return fill::zeros;
	}

	// Avoid recalculting cossine and sine functions
	double cosBetaPsi_ii{ cos(beta_ii - psi) }, cosBetaPsi_jj{ cos(beta_jj - psi) };
	double sinBetaPsi_ii{ sin(beta_ii - psi) }, sinBetaPsi_jj{ sin(beta_jj - psi) };

	// The integration is different for a horizontal cylinder, as the integration variable
	// along the cylinder is not related to the vertical position. This special case is treated separately.
	double mult_h_cos(0), mult_h_sin(0), mult_v_cos(0), mult_v_sin(0);;

	vec::fixed<2> km = { k_ii * cosBeta_ii - k_jj * cosBeta_jj, k_ii * sinBeta_ii - k_jj * sinBeta_jj };
	double k = arma::norm(km);
	double w = w_ii - w_jj;
	double I = tanAlpha * (k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj);
	double p1 = (k_ii*cosBeta_ii - k_jj * cosBeta_jj)*x1 + (k_ii*sinBeta_ii - k_jj * sinBeta_jj)*y1 + phase_ii - phase_jj;
	double p2 = (k_ii*cosBeta_ii - k_jj * cosBeta_jj)*x2 + (k_ii*sinBeta_ii - k_jj * sinBeta_jj)*y2 + phase_ii - phase_jj;
	double Ih = ((k_ii*cosBeta_ii - k_jj * cosBeta_jj)*(x2 - x1) + (k_ii*sinBeta_ii - k_jj * sinBeta_jj)*(y2 - y1)) / L;

	double aux = ((w_jj - w_ii) / (w_ii * w_jj)) * k_ii * k_jj * (std::cos(beta_ii - beta_jj) + std::tanh(k_ii*h) * std::tanh(k_jj*h))
		- 0.5 * (k_ii*k_ii / (w_ii * pow(std::cosh(k_ii*h), 2)) - k_jj * k_jj / (w_jj * pow(std::cosh(k_jj*h), 2)));
	aux = aux / (g * k * std::tanh(k * h) - w * w);
	double Q{ m_CM * 0.5 * A_ii * A_jj * g*g * aux * w }, Qz1{ Q * m_axialCa_1 / m_CM };
	double Qz2 = (evaluateTopNode ? Q * m_axialCa_2 / m_CM : 0);

	// Ratios of hyperbolic functions need a special treatment otherwise they yield inf in large water depth
	double cosh_sinh_1{ 0 }, cosh_sinh_2{ 0 }, cosh_cosh_1{ 0 }, cosh_cosh_2{ 0 }, sinh_sinh_1{ 0 }, sinh_sinh_2{ 0 }, sinh_cosh_1{ 0 }, sinh_cosh_2{ 0 };
	if (k*h >= 10)
	{
		cosh_sinh_1 = exp(k*z1);
		cosh_cosh_1 = cosh_sinh_1;
		sinh_sinh_1 = cosh_sinh_1;
		sinh_cosh_1 = cosh_sinh_1;

		cosh_sinh_2 = exp(k*z2);
		cosh_cosh_2 = cosh_sinh_2;
		sinh_sinh_2 = cosh_sinh_2;
		sinh_cosh_2 = cosh_sinh_2;
	}
	else
	{
		cosh_sinh_1 = cosh(k * (z1 + h)) / sinh(k*h);
		cosh_cosh_1 = cosh(k * (z1 + h)) / cosh(k*h);
		sinh_sinh_1 = sinh(k * (z1 + h)) / sinh(k*h);
		sinh_cosh_1 = sinh(k * (z1 + h)) / cosh(k*h);


		cosh_sinh_2 = cosh(k * (z2 + h)) / sinh(k*h);
		cosh_cosh_2 = cosh(k * (z2 + h)) / cosh(k*h);
		sinh_sinh_2 = sinh(k * (z2 + h)) / sinh(k*h);
		sinh_cosh_2 = sinh(k * (z2 + h)) / cosh(k*h);
	}

	/*
		FORCES
	*/
	if (arma::is_finite(tanAlpha))
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = k * sin(p2) * sinh_cosh_2 - I * cos(p2) * cosh_cosh_2;
		mult_h_cos += -k * sin(p1) * sinh_cosh_1 + I * cos(p1) * cosh_cosh_1;
		mult_h_cos *= 1 / (I*I + k * k);

		mult_h_sin = k * cos(p2) * sinh_cosh_2 + I * sin(p2) *cosh_cosh_2;
		mult_h_sin += -k * cos(p1) * sinh_cosh_1 - I * sin(p1) * cosh_cosh_1;
		mult_h_sin *= 1 / (I*I + k * k);

		// This is due to the change of integration variable.
		// We know cosAlpha !=0 because horizontal cylinders are treated separately
		mult_h_cos *= 1 / cosAlpha;
		mult_h_sin *= 1 / cosAlpha;

		// Contribution of the vertical acceleration
		mult_v_cos = k * cos(p2) * cosh_cosh_2 + I * sin(p2) * sinh_cosh_2;
		mult_v_cos += -k * cos(p1) * cosh_cosh_1 - I * sin(p1) * sinh_cosh_1;
		mult_v_cos *= 1 / (I*I + k * k);

		mult_v_sin = k * sin(p2) * cosh_cosh_2 - I * cos(p2) * sinh_cosh_2;
		mult_v_sin += -k * sin(p1) * cosh_cosh_1 + I * cos(p1) * sinh_cosh_1;
		mult_v_sin *= 1 / (I*I + k * k);

		mult_v_cos *= 1 / cosAlpha;
		mult_v_sin *= -1 / cosAlpha;
	}
	else
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = -cos(p2) + cos(p1);
		mult_h_cos *= 1 / Ih;

		mult_h_sin = sin(p2) - sin(p1);
		mult_h_sin *= 1 / Ih;

		// Contribution of the vertical acceleration
		mult_v_cos = mult_h_sin * sinh_cosh_1;
		mult_v_sin = -mult_h_cos * sinh_sinh_1;

		// Need the factor of the water depth in the horizontal acceleration as well
		mult_h_cos *= cosh_cosh_1;
		mult_h_sin *= cosh_cosh_1;
	}

	// Forces along the local x and y axes
	double fx_cos = Q * ((k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * cosAlpha * mult_h_cos + k * sinAlpha * mult_v_cos);
	double fx_sin = Q * ((k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * cosAlpha * mult_h_sin + k * sinAlpha * mult_v_sin);
	double fy_cos = Q * (k_ii*sinBetaPsi_ii - k_jj * sinBetaPsi_jj) * mult_h_cos;
	double fy_sin = Q * (k_ii*sinBetaPsi_ii - k_jj * sinBetaPsi_jj) * mult_h_sin;

	// Forces along the local z axis. Factor 4*R/3. due to this force being acceleration * volume, while in the end 
	// of this function things are multiplied by the area of the circle
	double fz_cos = (4 * R / 3.) * (k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * sinAlpha * (Qz1 * cosh_cosh_1 * sin(p1) - Qz2 * cosh_cosh_2 * sin(p2)); // Contribution of horizontal acceleration
	double fz_sin = (4 * R / 3.) * (k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * sinAlpha * (-Qz1 * cosh_cosh_1 * cos(p1) + Qz2 * cosh_cosh_2 * cos(p2));
	fz_cos += -(4 * R / 3.) * cosAlpha * k * (Qz1 * sinh_cosh_1 * cos(p1) - Qz2 * sinh_cosh_2 * cos(p2)); // Contribution of vertical acceleration
	fz_sin += (4 * R / 3.) * cosAlpha * k * (Qz1 * sinh_cosh_1 * sin(p1) - Qz2 * sinh_cosh_2 * sin(p2));

	/*
		MOMENTS
	*/
	if (arma::is_finite(tanAlpha))
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = k * sinh_cosh_2 * ((k*k + I * I)*(z2 - z1)*sin(p2) + 2 * I*cos(p2))
			+ cosh_cosh_2 * (-I * (k*k + I * I)*(z2 - z1)*cos(p2) + (I*I - k * k)*sin(p2));
		mult_h_cos += -k * sinh_cosh_1 * 2 * I*cos(p1)
			- cosh_cosh_1 * (I*I - k * k)*sin(p1);
		mult_h_cos *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_h_sin = k * sinh_cosh_2 * ((k*k + I * I)*(z2 - z1)*cos(p2) - 2 * I*sin(p2))
			+ cosh_cosh_2 * (I*(k*k + I * I)*(z2 - z1)*sin(p2) + (I*I - k * k)*cos(p2));
		mult_h_sin += k * sinh_cosh_1 * 2 * I*sin(p1)
			- cosh_cosh_1 * (I*I - k * k)*cos(p1);
		mult_h_sin *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_h_cos *= 1 / cosAlpha / cosAlpha;
		mult_h_sin *= 1 / cosAlpha / cosAlpha;

		// Contribution of the vertical acceleration
		mult_v_cos = k * cosh_cosh_2 * ((k*k + I * I)*(z2 - z1)*cos(p2) - 2 * I*sin(p2))
			+ sinh_cosh_2 * (I*(k*k + I * I)*(z2 - z1)*sin(p2) + (I*I - k * k)*cos(p2));
		mult_v_cos += k * cosh_cosh_1 * 2 * I*sin(p1)
			- sinh_cosh_1 * (I*I - k * k)*cos(p1);
		mult_v_cos *= 1 / (I*I + k * k) / (I*I + k * k);

		mult_v_sin = k * cosh_cosh_2 * ((k*k + I * I)*(z2 - z1)*sin(p2) + 2 * I*cos(p2))
			+ sinh_cosh_2 * (-I * (k*k + I * I)*(z2 - z1)*cos(p2) + (I*I - k * k)*sin(p2));
		mult_v_sin += -k * cosh_cosh_1 * 2 * I*cos(p1)
			- sinh_cosh_1 * (I*I - k * k)*sin(p1);
		mult_v_sin *= 1 / (I*I + k * k) / (I*I + k * k);

		// This is due to the change of integration variable
		mult_v_cos *= 1 / cosAlpha / cosAlpha;
		mult_v_sin *= -1 / cosAlpha / cosAlpha;
	}
	else
	{
		// Contribution of the horizontal acceleration
		mult_h_cos = -Ih * L*cos(p2) + sin(p2) - sin(p1);
		mult_h_cos *= 1 / (Ih*Ih);

		mult_h_sin = Ih * L*sin(p2) + cos(p2) - cos(p1);
		mult_h_sin *= 1 / (Ih*Ih);;

		// Contribution of the vertical acceleration
		mult_v_cos = mult_h_sin * sinh_cosh_1;
		mult_v_sin = -mult_h_cos * sinh_cosh_1;

		// Need the factor of the water depth in the horizontal acceleration as well
		mult_h_cos *= cosh_cosh_1;
		mult_h_sin *= cosh_cosh_1;
	}

	// Forces along the local x and y axes
	double mx_cos = -Q * (k_ii*sinBetaPsi_ii - k_jj * sinBetaPsi_jj) * mult_h_cos;
	double mx_sin = -Q * (k_ii*sinBetaPsi_ii - k_jj * sinBetaPsi_jj) * mult_h_sin;
	double my_cos = Q * ((k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * cosAlpha * mult_h_cos + k * sinAlpha * mult_v_cos);
	double my_sin = Q * ((k_ii*cosBetaPsi_ii - k_jj * cosBetaPsi_jj) * cosAlpha * mult_h_sin + k * sinAlpha * mult_v_sin);

	if (m_botPressFlag)
	{
		fz_cos += Q / m_CM * cosh_cosh_1 * cos(p1);
		fz_sin += Q / m_CM * cosh_cosh_1 * sin(p1);
		if (evaluateTopNode)
		{
			fz_cos -= Q / m_CM * cosh_cosh_2 * cos(p2);
			fz_sin -= Q / m_CM * cosh_cosh_2 * sin(p2);
		}
	}

	cx_vec::fixed<6> coef(fill::zeros);
	for (int ii = 0; ii < 3; ++ii)
	{
		coef.at(ii) = cx_double{ fx_cos * xvec(ii) + fy_cos * yvec(ii) + fz_cos * zvec(ii) , fx_sin * xvec(ii) + fy_sin * yvec(ii) + fz_sin * zvec(ii) };
	}
	for (int ii = 3; ii < 6; ++ii)
	{
		coef.at(ii) = cx_double{ mx_cos * xvec(ii - 3) + my_cos * yvec(ii - 3) , mx_sin * xvec(ii - 3) + my_sin * yvec(ii - 3) };
	}

	// Moments have to be with respect to the slow n1 position, as this is assumed by the other functions
	if (!m_flagFixed)
	{
		cx_vec::fixed<3> aux = { cross(n1 - node1Pos_sd(), real(coef.rows(0,2))), cross(n1 - node1Pos_sd(), imag(coef.rows(0,2))) };
		coef.rows(3, 5) += aux;
	}

	return rho * pi* R * R * coef;
}

vec::fixed<6> MorisonCirc::hydroForce_drag_already_calculated(const ENVIR & envir) const
{
	vec::fixed<6> force(fill::zeros);

	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };
	vec::fixed<3> v_axial = arma::dot(m_node1Vel, zvec) * zvec; // Since the cylinder is a rigid body, this is the same for all the nodes

	vec::fixed<3> u1(fill::zeros);
	int ncyl = m_nodesArray.n_cols;

	// Find indices for interpolation of the time vector
	const vec &t = envir.getTimeArray();
	uword ind1 = envir.getInd4interp1();
	uword ind2 = envir.getInd4interp2();

	for (int ii = 0; ii < ncyl; ++ii)
	{
		// Velocity of the integration point
		double lambda = norm(m_nodesArray.col(ii) - m_node1Pos_sd, 2) / norm(m_node2Pos_sd - m_node1Pos_sd);
		vec::fixed<3> vel_ii = m_node1Vel + lambda * (m_node2Vel - m_node1Vel);

		// Fluid velocity at the integration point.				
		double u_x = m_u1_Array_x.at(ind1, ii);
		double u_y = m_u1_Array_y.at(ind1, ii);
		double u_z = m_u1_Array_z.at(ind1, ii);
		if (envir.shouldInterp())
		{
			u_x += (m_u1_Array_x.at(ind2, ii) - m_u1_Array_x.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			u_y += (m_u1_Array_y.at(ind2, ii) - m_u1_Array_y.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			u_z += (m_u1_Array_z.at(ind2, ii) - m_u1_Array_z.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
		}
		u1 = { u_x, u_y, u_z };

		vec::fixed<3> u1_axial = arma::dot(u1, zvec) * zvec;
		if (ii == 0)
		{
			force.rows(0, 2) += 0.5 * envir.watDensity() * m_axialCD_1 * datum::pi * (m_diam*m_diam / 4.) * norm(u1_axial - v_axial, 2) * (u1_axial - v_axial);
		}
		else if (ii == ncyl - 1)
		{
			force.rows(0, 2) += 0.5 * envir.watDensity() * m_axialCD_2 * datum::pi * (m_diam*m_diam / 4.) * norm(u1_axial - v_axial, 2) * (u1_axial - v_axial);
		}

		// Quadratic drag forcealong the length
		u1 -= u1_axial;
		vec::fixed<3> force_ii = 0.5 * envir.watDensity() * m_CD * m_diam * norm(u1 - (vel_ii - v_axial), 2) * (u1 - (vel_ii - v_axial));


		// Integrate the forces along the cylinder using Simpson's Rule
		if (ii == 0 || ii == ncyl - 1)
		{
			force += (m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
		else if (ii % 2 != 0)
		{
			force += (4 * m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
		else
		{
			force += (2 * m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
	}

	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_drag_calculate(const ENVIR &envir) const
{
	vec::fixed<6> force(fill::zeros);
	vec::fixed<3> n1{ m_node1Pos_sd }, n2{ m_node2Pos_sd }, n_ii(fill::zeros);
	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };
	double eta = 0;

	if (n1.at(2) > 0)
		return force;

	// Evaluate immersed length and length discretization
	double Lw, dL;
	int ncyl;
	calculateImmersedLengthProperties_sd(Lw, ncyl, dL);

	vec::fixed<3> v_axial = arma::dot(m_node1Vel, zvec) * zvec; // Since the cylinder is a rigid body, this is the same for all the nodes
	for (int ii = 1; ii <= ncyl; ++ii)
	{
		n_ii = n1 + dL * (ii - 1) * zvec;

		if (n_ii[2] >= 0 && ii == ncyl)
		{
			n_ii[2] = 0;
		}

		if (envir.waveStret() == 2)
		{
			eta = envir.waveElev(n_ii.at(0), n_ii.at(1));
		}

		// Velocity of the integration point
		double lambda = norm(n_ii - n1, 2) / norm(n2 - n1);
		vec::fixed<3> vel_ii = m_node1Vel + lambda * (m_node2Vel - m_node1Vel);

		// Fluid velocity at the integration point.		
		vec::fixed<3> u1 = envir.u1(n_ii, eta);
		u1 -= arma::dot(u1, zvec) * zvec;

		// Quadratic drag force.
		vec::fixed<3> force_ii = 0.5 * envir.watDensity() * m_CD * m_diam * norm(u1 - (vel_ii - v_axial), 2) * (u1 - (vel_ii - v_axial));

		// Integrate the forces along the cylinder using Simpson's Rule
		if (ii == 1 || ii == ncyl)
		{
			force += (dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
		else if (ii % 2 == 0)
		{
			force += (4 * dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
		else
		{
			force += (2 * dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
	}

	if (n1.at(2) <= 0)
	{
		vec::fixed<3> u1 = arma::dot(envir.u1(n1, 0), zvec) * zvec;
		force.rows(0, 2) += 0.5 * envir.watDensity() * m_axialCD_1 * datum::pi * (m_diam*m_diam / 4.) * norm(u1 - v_axial, 2) * (u1 - v_axial);
	}

	if (n2.at(2) <= 0)
	{
		vec::fixed<3> u1 = arma::dot(envir.u1(n2, 0), zvec) * zvec;
		force.rows(0, 2) += 0.5 * envir.watDensity() * m_axialCD_2 * datum::pi * (m_diam*m_diam / 4.) * norm(u1 - v_axial, 2) * (u1 - v_axial);
	}

	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_convecAcc_already_calculated(const ENVIR & envir) const
{
	vec::fixed<6> force(fill::zeros);	

	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };

	vec::fixed<3> a_c(fill::zeros);
	vec::fixed<3> u(fill::zeros);
	vec::fixed<3> du1dx(fill::zeros);
	vec::fixed<3> du1dy(fill::zeros);
	vec::fixed<3> du1dz(fill::zeros);
	int ncyl = m_nodesArray.n_cols;

	// Find indices for interpolation of the time vector
	const vec &t = envir.getTimeArray();
	uword ind1 = envir.getInd4interp1();
	uword ind2 = envir.getInd4interp2();

	for (int ii = 0; ii < ncyl; ++ii)
	{

		// Fluid kinematics at the integration point.				
		double u_x = m_u1_Array_x.at(ind1, ii);
		double u_y = m_u1_Array_y.at(ind1, ii);
		double u_z = m_u1_Array_z.at(ind1, ii);		

		double du1dx_x = m_du1dx_Array_x.at(ind1, ii);
		double du1dx_y = m_du1dx_Array_y.at(ind1, ii);
		double du1dx_z = m_du1dx_Array_z.at(ind1, ii);
		double du1dy_x = m_du1dy_Array_x.at(ind1, ii);
		double du1dy_y = m_du1dy_Array_y.at(ind1, ii);
		double du1dy_z = m_du1dy_Array_z.at(ind1, ii);
		double du1dz_x = m_du1dz_Array_x.at(ind1, ii);
		double du1dz_y = m_du1dz_Array_y.at(ind1, ii);
		double du1dz_z = m_du1dz_Array_z.at(ind1, ii);
		if (envir.shouldInterp())
		{
			u_x += (m_u1_Array_x.at(ind2, ii) - m_u1_Array_x.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			u_y += (m_u1_Array_y.at(ind2, ii) - m_u1_Array_y.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			u_z += (m_u1_Array_z.at(ind2, ii) - m_u1_Array_z.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));

			du1dx_x += (m_du1dx_Array_x.at(ind2, ii) - m_du1dx_Array_x.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dx_y += (m_du1dx_Array_y.at(ind2, ii) - m_du1dx_Array_y.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dx_z += (m_du1dx_Array_z.at(ind2, ii) - m_du1dx_Array_z.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));

			du1dy_x += (m_du1dy_Array_x.at(ind2, ii) - m_du1dy_Array_x.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dy_y += (m_du1dy_Array_y.at(ind2, ii) - m_du1dy_Array_y.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dy_z += (m_du1dy_Array_z.at(ind2, ii) - m_du1dy_Array_z.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));

			du1dz_x += (m_du1dz_Array_x.at(ind2, ii) - m_du1dz_Array_x.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dz_y += (m_du1dz_Array_y.at(ind2, ii) - m_du1dz_Array_y.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
			du1dz_z += (m_du1dz_Array_z.at(ind2, ii) - m_du1dz_Array_z.at(ind1, ii)) * (envir.time() - t(ind1)) / (t(ind2) - t(ind1));
		}
		u = { u_x, u_y, u_z };
		du1dx = { du1dx_x, du1dx_y, du1dx_z };
		du1dy = { du1dy_x, du1dy_y, du1dy_z };
		du1dz = { du1dz_x, du1dz_y, du1dz_z };
		
		a_c.at(0) = u.at(0) * du1dx.at(0) + u.at(1) * du1dy.at(0) + u.at(2) * du1dz.at(0);
		a_c.at(1) = u.at(0) * du1dx.at(1) + u.at(1) * du1dy.at(1) + u.at(2) * du1dz.at(1);
		a_c.at(2) = u.at(0) * du1dx.at(2) + u.at(1) * du1dy.at(2) + u.at(2) * du1dz.at(2);
		vec::fixed<3> a_c_axial = arma::dot(a_c, zvec) * zvec;

		if (ii == 0)
		{
			force.rows(0, 2) += (4 / 3.) * datum::pi * (m_diam*m_diam*m_diam / 8.)  * envir.watDensity() * m_axialCa_1 * a_c_axial;
		}
		else if (ii == ncyl - 1)
		{
			force.rows(0, 2) += (4 / 3.) * datum::pi * (m_diam*m_diam*m_diam / 8.)  * envir.watDensity() * m_axialCa_2 * a_c_axial;
		}

		// Componente that is perpendicular to the cylinder axis
		a_c -= a_c_axial;
		vec::fixed<3> force_ii = datum::pi * (m_diam*m_diam / 4.) * envir.watDensity() * m_CM  * a_c;

		// Integrate the forces along the cylinder using Simpson's Rule
		if (ii == 0 || ii == ncyl - 1)
		{
			force += (m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
		else if (ii % 2 != 0)
		{
			force += (4 * m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
		else
		{
			force += (2 * m_dL / 3.0) * join_cols(force_ii, cross(m_nodesArray.col(ii) - m_node1Pos_sd, force_ii));
		}
	}

	return force;
}

vec::fixed<6> MorisonCirc::hydroForce_convecAcc_calculate(const ENVIR &envir) const
{
	vec::fixed<6> force(fill::zeros);
	vec::fixed<3> n1{ m_node1Pos_sd }, n2{ m_node2Pos_sd }, n_ii(fill::zeros);
	vec::fixed<3> xvec{ m_xvec_sd }, yvec{ m_yvec_sd }, zvec{ m_zvec_sd };
	vec::fixed<3> a_c(fill::zeros);
	double eta = 0;	

	// Evaluate immersed length and length discretization
	double Lw, dL;
	int ncyl;
	calculateImmersedLengthProperties_sd(Lw, ncyl, dL);

	vec::fixed<3> v_axial = arma::dot(m_node1Vel, zvec) * zvec; // Since the cylinder is a rigid body, this is the same for all the nodes
	for (int ii = 1; ii <= ncyl; ++ii)
	{
		n_ii = n1 + dL * (ii - 1) * zvec;

		if (n_ii[2] >= 0 && ii == ncyl)
		{
			n_ii[2] = 0;
		}

		if (envir.waveStret() == 2)
		{
			eta = envir.waveElev(n_ii.at(0), n_ii.at(1));
		}

		// Fluid kinematics at the integration point.		
		vec::fixed<3> u = envir.u1(n_ii, eta);
		vec::fixed<3> du1dx = envir.du1dx(n_ii, eta);
		vec::fixed<3> du1dy = envir.du1dy(n_ii, eta);
		vec::fixed<3> du1dz = envir.du1dz(n_ii, eta);

		// Quadratic drag force.		
		a_c.at(0) = u.at(0) * du1dx.at(0) + u.at(1) * du1dy.at(0) + u.at(2) * du1dz.at(0);
		a_c.at(1) = u.at(0) * du1dx.at(1) + u.at(1) * du1dy.at(1) + u.at(2) * du1dz.at(1);
		a_c.at(2) = u.at(0) * du1dx.at(2) + u.at(1) * du1dy.at(2) + u.at(2) * du1dz.at(2);
		vec::fixed<3> a_c_axial = arma::dot(a_c, zvec) * zvec;

		if (ii == 1)
		{
			force.rows(0, 2) += (4 / 3.) * datum::pi * (m_diam*m_diam*m_diam / 8.)  * envir.watDensity() * m_axialCa_1 * a_c_axial;
		}
		else if (ii == ncyl && n2.at(2) <= 0)
		{
			force.rows(0, 2) += (4 / 3.) * datum::pi * (m_diam*m_diam*m_diam / 8.)  * envir.watDensity() * m_axialCa_2 * a_c_axial;
		}

		// Componente that is perpendicular to the cylinder axis
		a_c -= a_c_axial;
		vec::fixed<3> force_ii = datum::pi * (m_diam*m_diam / 4.) * envir.watDensity() * m_CM  * a_c;

		// Integrate the forces along the cylinder using Simpson's Rule
		if (ii == 1 || ii == ncyl)
		{
			force += (dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
		else if (ii % 2 == 0)
		{
			force += (4 * dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
		else
		{
			force += (2 * dL / 3.0) * join_cols(force_ii, cross(n_ii - n1, force_ii));
		}
	}

	return force;
}

mat::fixed<6, 6> MorisonCirc::addedMass_perp(const double rho, const vec::fixed<3> &refPt, const int hydroMode) const
{
	mat::fixed<6, 6> A(fill::zeros);

	// Use a more friendly notation
	double Lambda = datum::pi * pow(m_diam / 2., 2) * rho * (m_CM - 1);
	int ncyl = m_numIntPoints;

	// Get vertical coordinate of the intersection with the waterline
	double zwl = 0;
	if (hydroMode == 2 && m_intersectWL.is_finite())
	{
		zwl = m_intersectWL.at(2);
	}

	// Nodes position and vectors of the local coordinate system vectors
	vec::fixed<3> n1 = node1Pos();
	vec::fixed<3> n2 = node2Pos();
	vec::fixed<3> xvec = m_xvec; // xvec and yvec are used to project the acceleration. They are analogous to the normal vector, which should consider the slow or fixed position
	vec::fixed<3> yvec = m_yvec;
	vec::fixed<3> zvec = m_zvec; // While zvec is used only to evaluate the nodes position, and hence should be first-order position
	if (hydroMode < 2)
	{
		n1 = node1Pos_sd();
		n2 = node2Pos_sd();
		xvec = m_xvec_sd;
		yvec = m_yvec_sd;
		zvec = m_zvec_sd;
	}

	// Since n2 is above n1, if n1[2] > zwl, the cylinder is above the waterline
	if (n1[2] > zwl)
	{
		return A;
	}

	// If only one of the nodes is above the water line, the coordinates of the other node
	// are changed by those of the intersection between the cylinder axis and the static
	// water line(defined by z_global = 0)
	if (n2[2] > zwl)
	{
		n2 = n1 + (std::abs(zwl - n1[2]) / (n2[2] - n1[2])) * norm(n2 - n1) * zvec;
	}

	// Length of the cylinder and of each interval between points
	double L = norm(n2 - n1, 2);
	double dL = norm(n2 - n1, 2) / (ncyl - 1);

	// The purely translational elements of the matrix (Aij for i,j = 1, 2, 3) are integrated analytically
	for (int pp = 0; pp < 3; ++pp)
	{
		for (int qq = pp; qq < 3; ++qq)
		{
			A(pp, qq) = Lambda * L * A_perp(pp, qq, { 0,0,0 }, refPt, xvec, yvec);
		}
	}

	vec::fixed<3> n_ii;
	double step{ 0 }; // Used for Simpson's rule. See below.
	//omp_set_num_threads(4);
	//#pragma omp parallel for
	for (int ii = 1; ii <= ncyl; ++ii)
	{
		n_ii = (n2 - n1) * (ii - 1) / (ncyl - 1) + n1; // Coordinates of the integration point

		if (n_ii[2] > zwl)
		{
			break; // Since n2 is above n1, if we reach n_ii[2] > 0, all the next n_ii are also above the waterline
		}

		// Integrate along the cylinder using Simpon's rule
		if (ii == 1 || ii == ncyl)
		{
			step = dL / 3.;
		}
		else if (ii % 2 == 0)
		{
			step = 4 * dL / 3.;
		}
		else
		{
			step = 2 * dL / 3.;
		}

		for (int pp = 0; pp < 6; ++pp)
		{
			int q0 = pp;
			if (pp < 3) // The first square of the matrix was already filled above
			{
				q0 = 3;
			}
			for (int qq = q0; qq < 6; ++qq)
			{
				A(pp, qq) += Lambda * step * A_perp(pp, qq, n_ii, refPt, xvec, yvec);
			}
		}
	}

	// The matrix is symmetrical. In the lines above, only the upper triangle was filled.
	// In the loop below, the lower triangle is filled with the values from the upper triangle.
	for (int pp = 0; pp < 6; ++pp)
	{
		for (int qq = 0; qq < pp; ++qq)
		{
			A(pp, qq) = A(qq, pp);
		}
	}

	return A;
}

mat::fixed<6, 6> MorisonCirc::addedMass_paral(const double rho, const vec::fixed<3> &refPt, const int hydroMode) const
{
	mat::fixed<6, 6> A(fill::zeros);

	// Nodes position and vectors of the local coordinate system vectors
	vec::fixed<3> n1 = node1Pos();
	vec::fixed<3> n2 = node2Pos();
	vec::fixed<3> zvec = m_zvec;
	if (hydroMode < 2)
	{
		n1 = node1Pos_sd();
		n2 = node2Pos_sd();
		zvec = m_zvec_sd;
	}

	// Center of Gravity
	double xG = refPt[0];
	double yG = refPt[1];
	double zG = refPt[2];

	if (n1[2] < 0)
	{
		for (int pp = 0; pp < 6; ++pp)
		{
			int q0 = pp;
			for (int qq = q0; qq < 6; ++qq)
			{
				A(pp, qq) += rho * m_axialCa_1 * (4 / 3.) * datum::pi * pow(m_diam / 2, 3) * A_paral(pp, qq, n1, refPt, zvec);
			}
		}
	}

	if (n2[2] < 0)
	{
		for (int pp = 0; pp < 6; ++pp)
		{
			int q0 = pp;
			for (int qq = q0; qq < 6; ++qq)
			{
				A(pp, qq) += rho * m_axialCa_2 * (4 / 3.) * datum::pi * pow(m_diam / 2, 3) * A_paral(pp, qq, n2, refPt, zvec);
			}
		}
	}

	// The matrix is symmetrical. In the lines above, only the upper triangle was filled.
	// In the loop below, the lower triangle is filled with the values from the upper triangle.
	for (int pp = 0; pp < 6; ++pp)
	{
		for (int qq = 0; qq < pp; ++qq)
		{
			A(pp, qq) = A(qq, pp);
		}
	}

	return A;
}

double MorisonCirc::A_perp(const int ii, const int jj, const vec::fixed<3> &x, const vec::fixed<3> &xG, const vec::fixed<3> &xvec, const vec::fixed<3> &yvec) const
{
	if (ii < 3 && jj < 3)
	{
		return (xvec.at(ii) * xvec.at(jj) + yvec.at(ii) * yvec.at(jj));
	}

	if (ii == 3 && jj == 3)
	{
		return (pow(x.at(1) - xG.at(1), 2) * (pow(xvec.at(2), 2) + pow(yvec.at(2), 2))
			+ pow(x.at(2) - xG.at(2), 2) * (pow(xvec.at(1), 2) + pow(yvec.at(1), 2))
			- 2 * (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2))  * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			);
	}

	if (ii == 4 && jj == 4)
	{
		return (pow(x.at(0) - xG.at(0), 2) * (pow(xvec.at(2), 2) + pow(yvec.at(2), 2))
			+ pow(x.at(2) - xG.at(2), 2) * (pow(xvec.at(0), 2) + pow(yvec.at(0), 2))
			- 2 * (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2))  * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			);
	}

	if (ii == 5 && jj == 5)
	{
		return (pow(x.at(0) - xG.at(0), 2) * (pow(xvec.at(1), 2) + pow(yvec.at(1), 2))
			+ pow(x.at(1) - xG.at(1), 2) * (pow(xvec.at(0), 2) + pow(yvec.at(0), 2))
			- 2 * (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1))  * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			);
	}

	if (ii == 0 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			- (x.at(2) - xG.at(2)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			);
	}

	if (ii == 0 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * (pow(xvec.at(0), 2) + pow(yvec.at(0), 2))
			- (x.at(0) - xG.at(0)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			);
	}

	if (ii == 0 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			- (x.at(1) - xG.at(1)) * (pow(xvec.at(0), 2) + pow(yvec.at(0), 2))
			);
	}

	if (ii == 1 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			- (x.at(2) - xG.at(2)) * (pow(xvec.at(1), 2) + pow(yvec.at(1), 2))
			);
	}

	if (ii == 1 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			- (x.at(0) - xG.at(0)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			);
	}

	if (ii == 1 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * (pow(xvec.at(1), 2) + pow(yvec.at(1), 2))
			- (x.at(1) - xG.at(1)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			);
	}

	if (ii == 2 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * (pow(xvec.at(2), 2) + pow(yvec.at(2), 2))
			- (x.at(2) - xG.at(2)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			);
	}

	if (ii == 2 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			- (x.at(0) - xG.at(0)) * (pow(xvec.at(2), 2) + pow(yvec.at(2), 2))
			);
	}

	if (ii == 2 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			- (x.at(1) - xG.at(1)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			);
	}

	if (ii == 3 && jj == 4)
	{
		return (-(x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * (pow(xvec.at(2), 2) + pow(yvec.at(2), 2))
			- (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			+ (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			- pow(x.at(2) - xG.at(2), 2) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			);
	}

	if (ii == 3 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			- (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2)) * (pow(xvec.at(1), 2) + pow(yvec.at(1), 2))
			- pow(x.at(1) - xG.at(1), 2) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			+ (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			);
	}

	if (ii == 4 && jj == 5)
	{
		return (-pow(x.at(0) - xG.at(0), 2) * (xvec.at(1) * xvec.at(2) + yvec.at(1) * yvec.at(2))
			+ (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * (xvec.at(0) * xvec.at(2) + yvec.at(0) * yvec.at(2))
			+ (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * (xvec.at(0) * xvec.at(1) + yvec.at(0) * yvec.at(1))
			- (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * (pow(xvec.at(0), 2) + pow(yvec.at(0), 2))
			);
	}

	return 0;
}

double MorisonCirc::A_paral(const int ii, const int jj, const vec::fixed<3> &x, const vec::fixed<3> &xG, const vec::fixed<3> &zvec) const
{
	if (ii < 3 && jj < 3)
	{
		return zvec(ii) * zvec(jj);
	}

	if (ii == 3 && jj == 3)
	{
		return (pow(x.at(1) - xG.at(1), 2) * pow(zvec.at(2), 2)
			+ pow(x.at(2) - xG.at(2), 2) * pow(zvec.at(1), 2)
			- 2 * (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * zvec.at(1) * zvec.at(2)
			);
	}

	if (ii == 4 && jj == 4)
	{
		return (pow(x.at(0) - xG.at(0), 2) * pow(zvec.at(2), 2)
			+ pow(x.at(2) - xG.at(2), 2) * pow(zvec.at(0), 2)
			- 2 * (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(2)
			);
	}

	if (ii == 5 && jj == 5)
	{
		return (pow(x.at(0) - xG.at(0), 2) * pow(zvec.at(1), 2)
			+ pow(x.at(1) - xG.at(1), 2) * pow(zvec.at(0), 2)
			- 2 * (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(1)
			);
	}

	if (ii == 0 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(2)
			- (x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(1));
	}

	if (ii == 0 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * pow(zvec.at(0), 2)
			- (x.at(0) - xG.at(0)) * zvec.at(0) * zvec.at(2));
	}

	if (ii == 0 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * zvec.at(0) * zvec.at(1)
			- (x.at(1) - xG.at(1)) * pow(zvec.at(0), 2));
	}

	if (ii == 1 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * zvec.at(1) * zvec.at(2)
			- (x.at(2) - xG.at(2)) * pow(zvec.at(1), 2));
	}

	if (ii == 1 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(1)
			- (x.at(0) - xG.at(0)) * zvec.at(1) * zvec.at(2));
	}

	if (ii == 1 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * pow(zvec.at(1), 2)
			- (x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(1));
	}

	if (ii == 2 && jj == 3)
	{
		return ((x.at(1) - xG.at(1)) * pow(zvec.at(2), 2)
			- (x.at(2) - xG.at(2)) * zvec.at(1) * zvec.at(2));
	}

	if (ii == 2 && jj == 4)
	{
		return ((x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(2)
			- (x.at(0) - xG.at(0)) * pow(zvec.at(2), 2));
	}

	if (ii == 2 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * zvec.at(1) * zvec.at(2)
			- (x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(2));
	}

	if (ii == 3 && jj == 4)
	{
		return (-(x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * pow(zvec.at(2), 2)
			- (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2)) * zvec.at(1) * zvec.at(2)
			+ (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(2)
			- pow(x.at(2) - xG.at(2), 2) * zvec.at(0) * zvec.at(1));
	}

	if (ii == 3 && jj == 5)
	{
		return ((x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * zvec.at(1) * zvec.at(2)
			- (x.at(0) - xG.at(0)) * (x.at(2) - xG.at(2)) * pow(zvec.at(1), 2)
			- pow(x.at(1) - xG.at(1), 2) * zvec.at(0) * zvec.at(2)
			+ (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * zvec.at(0) * zvec.at(1));
	}

	if (ii == 4 && jj == 5)
	{
		return (-pow(x.at(0) - xG.at(0), 2) * zvec.at(1) * zvec.at(2)
			+ (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(2)
			+ (x.at(0) - xG.at(0)) * (x.at(1) - xG.at(1)) * zvec.at(0) * zvec.at(1)
			- (x.at(1) - xG.at(1)) * (x.at(2) - xG.at(2)) * pow(zvec.at(0), 2));
	}

	return 0;
}

/*****************************************************
	Printing
*****************************************************/
std::string MorisonCirc::print() const
{
	std::string output = "";

	output = output + "CoG_2_node1:\t(" + std::to_string(m_cog2node1(0)) + ", " + std::to_string(m_cog2node1(1)) + ", " + std::to_string(m_cog2node1(2)) + ")\n";
	output = output + "CoG_2_node1:\t(" + std::to_string(m_cog2node2(0)) + ", " + std::to_string(m_cog2node2(1)) + ", " + std::to_string(m_cog2node2(2)) + ")\n";
	output = output + "node1 at t=0:\t(" + std::to_string(m_node1Pos(0)) + ", " + std::to_string(m_node1Pos(1)) + ", " + std::to_string(m_node1Pos(2)) + ")\n";
	output = output + "node2 at t=0:\t(" + std::to_string(m_node2Pos(0)) + ", " + std::to_string(m_node2Pos(1)) + ", " + std::to_string(m_node2Pos(2)) + ")\n";
	output = output + "Diameter:\t" + std::to_string(m_diam) + '\n';
	output = output + "Drag Coeff.:\t" + std::to_string(m_CD) + '\n';
	output = output + "Inert. Coeff.:\t" + std::to_string(m_CM) + '\n';
	output = output + "Numb. of Int. Points:\t" + std::to_string(m_numIntPoints) + '\n';
	output = output + "Axial CD - Node 1:\t" + std::to_string(m_axialCD_1) + '\n';
	output = output + "Axial Ca - Node 1:\t" + std::to_string(m_axialCa_1) + '\n';
	output = output + "Axial CD - Node 2:\t" + std::to_string(m_axialCD_2) + '\n';
	output = output + "Axial Ca - Node 2:\t" + std::to_string(m_axialCa_2) + '\n';
	output = output + "Bot. Press. Flag.:\t" + std::to_string(m_botPressFlag) + '\n';

	return output;
}


/*****************************************************
	Clone for creating copies of the Morison Element
*****************************************************/
MorisonCirc* MorisonCirc::clone() const
{
	return (new MorisonCirc(*this));
}
