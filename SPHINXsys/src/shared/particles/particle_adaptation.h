/* -------------------------------------------------------------------------*
*								SPHinXsys									*
* --------------------------------------------------------------------------*
* SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle	*
* Hydrodynamics for industrial compleX systems. It provides C++ APIs for	*
* physical accurate simulation and aims to model coupled industrial dynamic *
* systems including fluid, solid, multi-body dynamics and beyond with SPH	*
* (smoothed particle hydrodynamics), a meshless computational method using	*
* particle discretization.													*
*																			*
* SPHinXsys is partially funded by German Research Foundation				*
* (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1				*
* and HU1527/12-1.															*
*                                                                           *
* Portions copyright (c) 2017-2020 Technical University of Munich and		*
* the authors' affiliations.												*
*                                                                           *
* Licensed under the Apache License, Version 2.0 (the "License"); you may   *
* not use this file except in compliance with the License. You may obtain a *
* copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
*                                                                           *
* --------------------------------------------------------------------------*/
/**
 * @file 	particle_adaptation.h
 * @brief 	Particle adaptation is for adaptivity of SPH particles.
 *			The particle adaptation is defined before SPH body.
 * @author	Xiangyu Hu and Chi Zhang
 */


#ifndef PARTICLE_ADAPTATION_H
#define PARTICLE_ADAPTATION_H



#include "base_data_package.h"
#include "sph_data_conainers.h"


namespace SPH {

	class SPHBody;
	class ComplexShape;
	class Kernel;
	class BaseMeshCellLinkedList;
	class BaseLevelSet;
	class BaseParticles;

	/**
	 * @class ParticleAdaptation
	 * @brief Base class for particle adaptation
	 * The base class defined essential global parameteres.
	 * It is also used for single resolution SPH method.
	 * The derived class will be used if further adaptation is introduced.
	 */
	class ParticleAdaptation
	{
	protected:
		Real h_spacing_ratio_;
		int global_refinement_level_;
		Real system_resolution_ratio_; // ratio of body resolution to system resolution, set to 1.0 by default
		int local_refinement_level_;
		int local_coarse_level_;
		Real spacing_ref_;
		Real Vol_ref_;
		Real h_ref_;
		Real spacing_min_;
		Real spacing_ratio_min_;
		Real spacing_ratio_max_;
		Real h_ratio_min_;
		Real h_ratio_max_;
		Real number_density_min_;
		Real number_density_max_;
		

		Kernel* kernel_;
		SPHBody* sph_body_;
		BoundingBox system_domain_bounds_;
		BaseParticles* base_particles_;
	public:
		ParticleAdaptation(Real h_spacing_ratio = 1.3, int global_refinement_level = 0);
		virtual ~ParticleAdaptation() {};
		/** Note: called  after construction all derived classes. */
		virtual void initialize(SPHBody* sph_body);

		int GlobalRefinementLevel() { return global_refinement_level_; };
		int LocalRefinementLevel() { return local_refinement_level_; };
		Real ReferenceSpacing() { return spacing_ref_; };
		Real ReferenceVolume() { return Vol_ref_; };
		Real ReferenceSmoothingLength() { return h_ref_; };
		Kernel* getKernel() { return kernel_; };
		/**replace a kernel should be done before kernel initialization */
		void replaceKernel(Kernel* another_kernel);
		Real MinimumSpacing() { return spacing_min_; };
		Real MinimumSpacingRatio() { return spacing_ratio_min_; };
		Real MaximumSpacingRatio() { return spacing_ratio_max_; };
		Real computeReferenceNumberDensity(Vec2d zero, Real h_ratio);
		Real computeReferenceNumberDensity(Vec3d zero, Real h_ratio);
		Real ReferenceNumberDensity();
		Real probeNumberDensity(Vecd zero, Real h_ratio);
		virtual Real SmoothingLengthRatio(size_t particle_index_i) {return 1.0; };

		virtual void assignBaseParticles(BaseParticles* base_particles);
		virtual BaseMeshCellLinkedList* createMeshCellLinkedList();
		virtual BaseLevelSet* createLevelSet(ComplexShape& complex_shape);

		void SetSystemResolutionRatio(Real system_resolution_ratio) { system_resolution_ratio_ = system_resolution_ratio; };
	protected:
		Real RefinedSpacing(Real coarse_particle_spacing, int refinement_level);
	};

	/**
	 * @class ParticleWithLocalRefinement
	 * @brief Base class for particle with refinement.
	 */
	class ParticleWithLocalRefinement : public ParticleAdaptation
	{
	public:
		StdLargeVec<Real> h_ratio_;	/**< the ratio between reference smoothing length to variable smoothing length */

		ParticleWithLocalRefinement(Real h_spacing_ratio_,
			int global_refinement_level, int local_refinement_level);
		virtual ~ParticleWithLocalRefinement() {};

		size_t getMeshCellLinkedListTotalLevel();
		size_t getLevelSetTotalLevel();
		virtual Real SmoothingLengthRatio(size_t particle_index_i) override {return h_ratio_[particle_index_i]; };

		virtual void assignBaseParticles(BaseParticles* base_particles) override;
		virtual BaseMeshCellLinkedList* createMeshCellLinkedList() override;
		virtual BaseLevelSet* createLevelSet(ComplexShape& complex_shape) override;
	};
	/**
	 * @class ParticleSpacingByBodyShape
	 * @brief Adaptive resolutions within a SPH body according to the distance to the body surface.
	 */
	class ParticleSpacingByBodyShape : public ParticleWithLocalRefinement
	{
	public:
		ParticleSpacingByBodyShape(Real smoothing_length_ratio,
			int global_refinement_level, int local_refinement_level);
		virtual ~ParticleSpacingByBodyShape() {};

		Real getLocalSpacing(ComplexShape& complex_shape, Vecd& position);
	};
	
	/**
	* @class ShellParticleAdaptation
	* @brief Base class for ShellParticleAdaptation.
	*/
	class ShellParticleAdaptation : public ParticleAdaptation
	{
	protected:
		Real global_avg_thickness_;
	public:
		ShellParticleAdaptation();
		virtual ~ShellParticleAdaptation() {};

		Real getGlobalAvgThickness(){ return global_avg_thickness_; };
		Real RefinedReferenceSpacing() { return global_avg_thickness_ * 0.25; };
		virtual BaseLevelSet* createLevelSet(ComplexShape& complex_shape) override;
	};
}
#endif //PARTICLE_ADAPTATION_H