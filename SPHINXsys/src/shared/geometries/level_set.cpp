/**
 * @file 	level_set.cpp
 * @author	Luhui Han, Chi ZHang and Xiangyu Hu
 */

#include "level_set.h"
#include "base_body.h"
#include "adaptation.h"
#include "mesh_with_data_packages.hpp"

namespace SPH
{
	//=================================================================================================//
	LevelSetDataPackage::
		LevelSetDataPackage() : BaseDataPackage<4, 6>(), is_core_pkg_(false) {}
	//=================================================================================================//
	void LevelSetDataPackage::initializeSingularDataAddress()
	{
		initializePackageDataAddress(phi_, phi_addrs_);
		initializePackageDataAddress(n_, n_addrs_);
		initializePackageDataAddress(none_normalized_n_, none_normalized_n_addrs_);
		initializePackageDataAddress(kernel_weight_, kernel_weight_addrs_);
		initializePackageDataAddress(kernel_gradient_, kernel_gradient_addrs_);
		initializePackageDataAddress(near_interface_id_, near_interface_id_addrs_);
	}
	//=================================================================================================//
	void LevelSetDataPackage::
		assignAllPackageDataAddress(Vecu addrs_index, LevelSetDataPackage *src_pkg, Vecu data_index)
	{
		assignPackageDataAddress(phi_addrs_, addrs_index, src_pkg->phi_, data_index);
		assignPackageDataAddress(n_addrs_, addrs_index, src_pkg->n_, data_index);
		assignPackageDataAddress(none_normalized_n_addrs_, addrs_index, src_pkg->none_normalized_n_, data_index);
		assignPackageDataAddress(kernel_weight_addrs_, addrs_index, src_pkg->kernel_weight_, data_index);
		assignPackageDataAddress(kernel_gradient_addrs_, addrs_index, src_pkg->kernel_gradient_, data_index);
		assignPackageDataAddress(near_interface_id_addrs_, addrs_index, src_pkg->near_interface_id_, data_index);
	}
	//=================================================================================================//
	void LevelSetDataPackage::computeNormalDirection()
	{
		computeNormalizedGradient(phi_addrs_, n_addrs_);
	}
	//=================================================================================================//
	void LevelSetDataPackage::computeNoneNormalizedNormalDirection()
	{
		computeGradient(phi_addrs_, none_normalized_n_addrs_);
	}
	//=================================================================================================//
	BaseLevelSet ::BaseLevelSet(Shape &shape, SPHAdaptation &sph_adaptation)
		: BaseMeshField("LevelSet"),
		  shape_(shape), sph_adaptation_(sph_adaptation) {}
	//=================================================================================================//
	Real BaseLevelSet::computeHeaviside(Real phi, Real half_width)
	{
		Real heaviside = 0.0;
		Real normalized_phi = phi / half_width;
		if (phi < half_width && phi > -half_width)
			heaviside = (0.5 + 0.5 * normalized_phi) + 0.5 * sin(Pi * normalized_phi) / Pi;
		if (normalized_phi > 1.0)
			heaviside = 1.0;
		return heaviside;
	}
	//=================================================================================================//
	LevelSet::LevelSet(BoundingBox tentative_bounds, Real data_spacing, size_t buffer_size,
					   Shape &shape, SPHAdaptation &sph_adaptation)
		: MeshWithDataPackages<BaseLevelSet, LevelSetDataPackage>(tentative_bounds, data_spacing, buffer_size,
																  shape, sph_adaptation),
		  global_h_ratio_(sph_adaptation.ReferenceSpacing() / data_spacing),
		  small_shift_factor_(sph_adaptation.SmallShiftFactor()),
		  kernel_(*sph_adaptation.getKernel())
	{
		Real far_field_distance = grid_spacing_ * (Real)buffer_width_;
		initializeASingularDataPackage(-far_field_distance);
		initializeASingularDataPackage(far_field_distance);
	}
	//=================================================================================================//
	LevelSet::LevelSet(BoundingBox tentative_bounds, Real data_spacing,
					   Shape &shape, SPHAdaptation &sph_adaptation)
		: LevelSet(tentative_bounds, data_spacing, 4, shape, sph_adaptation)
	{
		MeshFunctor initialize_data_in_a_cell = std::bind(&LevelSet::initializeDataInACell, this, _1, _2);
		MeshIterator_parallel(Vecu(0), number_of_cells_, initialize_data_in_a_cell);
		finishDataPackages();
	}
	//=================================================================================================//
	void LevelSet::finishDataPackages()
	{
		MeshFunctor tag_a_cell_inner_pkg = std::bind(&LevelSet::tagACellIsInnerPackage, this, _1, _2);
		MeshIterator_parallel(Vecu(0), number_of_cells_, tag_a_cell_inner_pkg);
		MeshFunctor initial_address_in_a_cell = std::bind(&LevelSet::initializeAddressesInACell, this, _1, _2);
		MeshIterator_parallel(Vecu(0), number_of_cells_, initial_address_in_a_cell);
		updateNormalDirection();
		updateNoneNormalizedNormalDirection();
		updateKernelIntegrals();
	}
	//=================================================================================================//
	void LevelSet::initializeAddressesInACell(const Vecu &cell_index, Real dt)
	{
		initializePackageAddressesInACell(cell_index);
	}
	//=================================================================================================//
	void LevelSet::updateNormalDirection()
	{
		PackageFunctor<void, LevelSetDataPackage> update_normal_diraction =
			std::bind(&LevelSet::updateNormalDirectionForAPackage, this, _1, _2);
		PackageIterator_parallel<LevelSetDataPackage>(inner_data_pkgs_, update_normal_diraction);
	}
	//=================================================================================================//
	void LevelSet::updateNoneNormalizedNormalDirection()
	{
		PackageFunctor<void, LevelSetDataPackage> update_none_normalized_normal_diraction =
			std::bind(&LevelSet::updateNoneNormalizedNormalDirectionForAPackage, this, _1, _2);
		PackageIterator_parallel<LevelSetDataPackage>(inner_data_pkgs_, update_none_normalized_normal_diraction);
	}
	//=================================================================================================//
	void LevelSet::updateKernelIntegrals()
	{
		PackageFunctor<void, LevelSetDataPackage> update_kernel_value =
			std::bind(&LevelSet::updateKernelIntegralsForAPackage, this, _1, _2);
		PackageIterator_parallel<LevelSetDataPackage>(inner_data_pkgs_, update_kernel_value);
	}
	//=================================================================================================//
	Vecd LevelSet::probeNormalDirection(const Vecd &position)
	{
		return probeMesh<Vecd, LevelSetDataPackage::PackageDataAddress<Vecd>,
						 &LevelSetDataPackage::n_addrs_>(position);
	}
	//=================================================================================================//
	Vecd LevelSet::probeNoneNormalizedNormalDirection(const Vecd &position)
	{
		return probeMesh<Vecd, LevelSetDataPackage::PackageDataAddress<Vecd>,
						 &LevelSetDataPackage::none_normalized_n_addrs_>(position);
	}
	//=================================================================================================//
	Real LevelSet::probeSignedDistance(const Vecd &position)
	{
		return probeMesh<Real, LevelSetDataPackage::PackageDataAddress<Real>,
						 &LevelSetDataPackage::phi_addrs_>(position);
	}
	//=================================================================================================//
	Real LevelSet::probeKernelIntegral(const Vecd &position, Real h_ratio)
	{
		return probeMesh<Real, LevelSetDataPackage::PackageDataAddress<Real>,
						 &LevelSetDataPackage::kernel_weight_addrs_>(position);
	}
	//=================================================================================================//
	Vecd LevelSet::probeKernelGradientIntegral(const Vecd &position, Real h_ratio)
	{
		return probeMesh<Vecd, LevelSetDataPackage::PackageDataAddress<Vecd>,
						 &LevelSetDataPackage::kernel_gradient_addrs_>(position);
	}
	//=================================================================================================//
	void LevelSet::
		updateNormalDirectionForAPackage(LevelSetDataPackage *inner_data_pkg, Real dt)
	{
		inner_data_pkg->computeNormalDirection();
	}
	//=================================================================================================//
	void LevelSet::
		updateNoneNormalizedNormalDirectionForAPackage(LevelSetDataPackage *inner_data_pkg, Real dt)
	{
		inner_data_pkg->computeNoneNormalizedNormalDirection();
	}
	//=================================================================================================//
	void LevelSet::
		updateKernelIntegralsForAPackage(LevelSetDataPackage *inner_data_pkg, Real dt)
	{
		inner_data_pkg->computeKernelIntegrals(*this);
	}
	//=================================================================================================//
	void LevelSet::
		stepReinitializationForAPackage(LevelSetDataPackage *inner_data_pkg, Real dt)
	{
		inner_data_pkg->stepReinitialization();
	}
	//=============================================================================================//
	void LevelSet::reinitializeLevelSet()
	{
		PackageFunctor<void, LevelSetDataPackage> reinitialize_levelset =
			std::bind(&LevelSet::stepReinitializationForAPackage, this, _1, _2);
		for (size_t i = 0; i < 50; ++i)
			PackageIterator_parallel<LevelSetDataPackage>(inner_data_pkgs_, reinitialize_levelset);
	}
	//=================================================================================================//
	void LevelSet::markNearInterface()
	{
		PackageFunctor<void, LevelSetDataPackage> mark_cutcell_by_levelset =
			std::bind(&LevelSet::markNearInterfaceForAPackage, this, _1, _2);
		PackageIterator_parallel<LevelSetDataPackage>(core_data_pkgs_, mark_cutcell_by_levelset);
	}
	//=================================================================================================//
	void LevelSet::markNearInterfaceForAPackage(LevelSetDataPackage *core_data_pkg, Real dt)
	{
		core_data_pkg->markNearInterface(small_shift_factor_);
	}
	//=================================================================================================//
	void LevelSet::redistanceInterface()
	{
		PackageFunctor<void, LevelSetDataPackage> clean_levelset =
			std::bind(&LevelSet::redistanceInterfaceForAPackage, this, _1, _2);
		PackageIterator_parallel<LevelSetDataPackage>(core_data_pkgs_, clean_levelset);
	}
	//=================================================================================================//
	void LevelSet::cleanInterface(bool isSmoothed)
	{
		markNearInterface();
		redistanceInterface();
		reinitializeLevelSet();
		updateNormalDirection();
		updateKernelIntegrals();
	}
	//=================================================================================================//
	bool LevelSet::probeIsWithinMeshBound(const Vecd &position)
	{
		bool is_bounded = true;
		Vecu cell_pos = CellIndexFromPosition(position);
		for (int i = 0; i != position.size(); ++i)
		{
			if (cell_pos[i] < 2)
				is_bounded = false;
			if (cell_pos[i] > (number_of_cells_[i] - 2))
				is_bounded = false;
		}
		return is_bounded;
	}
	//=================================================================================================//
	LevelSetDataPackage *LevelSet::createDataPackage(const Vecu &cell_index, const Vecd &cell_position)
	{
		mutex_my_pool.lock();
		LevelSetDataPackage *new_data_pkg = data_pkg_pool_.malloc();
		mutex_my_pool.unlock();
		Vecd pkg_lower_bound = GridPositionFromCellPosition(cell_position);
		new_data_pkg->initializePackageGeometry(pkg_lower_bound, data_spacing_);
		new_data_pkg->initializeBasicData(shape_);
		new_data_pkg->pkg_index_ = cell_index;
		assignDataPackageAddress(cell_index, new_data_pkg);
		return new_data_pkg;
	}
	//=================================================================================================//
	void LevelSet::initializeDataInACell(const Vecu &cell_index, Real dt)
	{
		Vecd cell_position = CellPositionFromIndex(cell_index);
		Real signed_distance = shape_.findSignedDistance(cell_position);
		Vecd normal_direction = shape_.findNormalDirection(cell_position);
		Real measure = getMaxAbsoluteElement(normal_direction * signed_distance);
		if (measure < grid_spacing_)
		{
			LevelSetDataPackage *new_data_pkg = createDataPackage(cell_index, cell_position);
			new_data_pkg->is_core_pkg_ = true;
			core_data_pkgs_.push_back(new_data_pkg);
		}
		else
		{
			LevelSetDataPackage *singular_data_pkg =
				shape_.checkContain(cell_position) ? singular_data_pkgs_addrs_[0] : singular_data_pkgs_addrs_[1];
			assignDataPackageAddress(cell_index, singular_data_pkg);
		}
	}
	//=============================================================================================//
	void LevelSet::tagACellIsInnerPackage(const Vecu &cell_index, Real dt)
	{
		bool is_inner_pkg = isInnerPackage(cell_index);

		if (is_inner_pkg)
		{
			LevelSetDataPackage *current_data_pkg = DataPackageFromCellIndex(cell_index);
			if (current_data_pkg->is_core_pkg_)
			{
				current_data_pkg->is_inner_pkg_ = true;
				inner_data_pkgs_.push_back(current_data_pkg);
			}
			else
			{
				Vecd cell_position = CellPositionFromIndex(cell_index);
				LevelSetDataPackage *new_data_pkg = createDataPackage(cell_index, cell_position);
				new_data_pkg->is_inner_pkg_ = true;
				inner_data_pkgs_.push_back(new_data_pkg);
			}
		}
	}
	//=============================================================================================//
	RefinedLevelSet::RefinedLevelSet(BoundingBox tentative_bounds, LevelSet &coarse_level_set,
									 Shape &shape, SPHAdaptation &sph_adaptation)
		: RefinedMesh(tentative_bounds, coarse_level_set, 4, shape, sph_adaptation)
	{
		MeshFunctor initialize_data_in_a_cell = std::bind(&RefinedLevelSet::initializeDataInACellFromCoarse, this, _1, _2);
		MeshIterator_parallel(Vecu(0), number_of_cells_, initialize_data_in_a_cell);
		finishDataPackages();
	}
	//=============================================================================================//
	void RefinedLevelSet::initializeDataInACellFromCoarse(const Vecu &cell_index, Real dt)
	{
		Vecd cell_position = CellPositionFromIndex(cell_index);
		LevelSetDataPackage *singular_data_pkg = coarse_mesh_.probeSignedDistance(cell_position) < 0.0
													 ? singular_data_pkgs_addrs_[0]
													 : singular_data_pkgs_addrs_[1];
		assignDataPackageAddress(cell_index, singular_data_pkg);
		if (coarse_mesh_.isWithinCorePackage(cell_position))
		{
			Real signed_distance = shape_.findSignedDistance(cell_position);
			Vecd normal_direction = shape_.findNormalDirection(cell_position);
			Real measure = getMaxAbsoluteElement(normal_direction * signed_distance);
			if (measure < grid_spacing_)
			{
				LevelSetDataPackage *new_data_pkg = createDataPackage(cell_index, cell_position);
				new_data_pkg->is_core_pkg_ = true;
				core_data_pkgs_.push_back(new_data_pkg);
			}
		}
	}
	//=============================================================================================//
	MultilevelLevelSet::
		MultilevelLevelSet(BoundingBox tentative_bounds, Real reference_data_spacing,
						   size_t total_levels, Shape &shape, SPHAdaptation &sph_adaptation)
		: MultilevelMesh<BaseLevelSet, LevelSet, RefinedLevelSet>(tentative_bounds, reference_data_spacing,
																  total_levels, shape, sph_adaptation) {}
	//=================================================================================================//
	size_t MultilevelLevelSet::getMeshLevel(Real h_ratio)
	{
		for (size_t level = total_levels_; level != 0; --level)
			if (h_ratio - mesh_levels_[level - 1]->global_h_ratio_ > -Eps)
				return level - 1; // jump out the loop!

		std::cout << "\n Error: LevelSet level searching out of bound!" << std::endl;
		std::cout << __FILE__ << ':' << __LINE__ << std::endl;
		exit(1);
		return 999; // means an error in level searching
	};
	//=================================================================================================//
	void MultilevelLevelSet::cleanInterface(bool isSmoothed)
	{
		mesh_levels_.back()->cleanInterface();
	}
	//=============================================================================================//
	Real MultilevelLevelSet::probeSignedDistance(const Vecd &position)
	{
		return mesh_levels_[getProbeLevel(position)]->probeSignedDistance(position);
	}
	//=============================================================================================//
	Vecd MultilevelLevelSet::probeNormalDirection(const Vecd &position)
	{
		return mesh_levels_[getProbeLevel(position)]->probeNormalDirection(position);
	}
	//=============================================================================================//
	Vecd MultilevelLevelSet::probeNoneNormalizedNormalDirection(const Vecd &position)
	{
		return mesh_levels_[getProbeLevel(position)]->probeNoneNormalizedNormalDirection(position);
	}
	//=============================================================================================//
	size_t MultilevelLevelSet::getProbeLevel(const Vecd &position)
	{
		for (size_t level = total_levels_; level != 0; --level)
			if (mesh_levels_[level - 1]->isWithinCorePackage(position))
				return level - 1; // jump out of the loop!
		return 0;
	}
	//=================================================================================================//
	Real MultilevelLevelSet::probeKernelIntegral(const Vecd &position, Real h_ratio)
	{
		size_t coarse_level = getMeshLevel(h_ratio);
		Real alpha = (mesh_levels_[coarse_level + 1]->global_h_ratio_ - h_ratio) /
					 (mesh_levels_[coarse_level + 1]->global_h_ratio_ - mesh_levels_[coarse_level]->global_h_ratio_);
		Real coarse_level_value = mesh_levels_[coarse_level]->probeKernelIntegral(position);
		Real fine_level_value = mesh_levels_[coarse_level + 1]->probeKernelIntegral(position);

		return alpha * coarse_level_value + (1.0 - alpha) * fine_level_value;
	}
	//=================================================================================================//
	Vecd MultilevelLevelSet::probeKernelGradientIntegral(const Vecd &position, Real h_ratio)
	{
		size_t coarse_level = getMeshLevel(h_ratio);
		Real alpha = (mesh_levels_[coarse_level + 1]->global_h_ratio_ - h_ratio) /
					 (mesh_levels_[coarse_level + 1]->global_h_ratio_ - mesh_levels_[coarse_level]->global_h_ratio_);
		Vecd coarse_level_value = mesh_levels_[coarse_level]->probeKernelGradientIntegral(position);
		Vecd fine_level_value = mesh_levels_[coarse_level + 1]->probeKernelGradientIntegral(position);

		return alpha * coarse_level_value + (1.0 - alpha) * fine_level_value;
	}
	//=================================================================================================//
	bool MultilevelLevelSet::probeIsWithinMeshBound(const Vecd &position)
	{
		bool is_bounded = true;
		for (size_t l = 0; l != total_levels_; ++l)
		{
			if (!mesh_levels_[l]->probeIsWithinMeshBound(position))
			{
				is_bounded = false;
				break;
			};
		}
		return is_bounded;
	}
	//=============================================================================================//
}
