#ifndef _QUDA_MILC_INTERFACE_H
#define _QUDA_MILC_INTERFACE_H

#include <enum_quda.h>
#include <quda.h>

/**
 * @file    quda_milc_interface.h
 *
 * @section Description
 *
 * The header file defines the milc interface to enable easy
 * interfacing between QUDA and the MILC software packed.
 */
#if defined(QUDA_TARGET_CUDA)
#if __COMPUTE_CAPABILITY__ >= 600
#define USE_QUDA_MANAGED 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Parameters related to MILC site struct
   */
  typedef struct {
    void *site; /** Pointer to beginning of site array */
    void *link; /** Pointer to link field (only used if site is not set) */
    size_t link_offset; /** Offset to link entry in site struct (bytes) */
    void *mom; /** Pointer to link field (only used if site is not set) */
    size_t mom_offset; /** Offset to mom entry in site struct (bytes) */
    size_t size; /** Size of site struct (bytes) */
  } QudaMILCSiteArg_t;

  /**
   * Parameters related to linear solvers.
   */
  typedef struct {
    int max_iter; /** Maximum number of iterations */
    QudaParity evenodd; /** Which parity are we working on ? (options are QUDA_EVEN_PARITY, QUDA_ODD_PARITY, QUDA_INVALID_PARITY */
    int mixed_precision; /** Whether to use mixed precision or not (1 - yes, 0 - no) */
    double boundary_phase[4]; /** Boundary conditions */
    int make_resident_solution; /** Make the solution resident and don't copy back */
    int use_resident_solution; /** Use the resident solution */
    QudaInverterType solver_type; /** Type of solver to use */
    double tadpole; /** Tadpole improvement factor - set to 1.0 for
                        HISQ fermions since the tadpole factor is
                        baked into the links during their construction */
    double naik_epsilon; /** Naik epsilon parameter (HISQ fermions only).*/

    QudaEigParam eig_param; /** To pass deflation-related stuff like eigenvector filename **/
    double tol_restart;
    QudaPrecision prec_eigensolver;
  } QudaInvertArgs_t;

  /**
   * Parameters related to deflated solvers.
   */

  typedef struct {
    QudaPrecision  prec_ritz;
    int nev;
    int max_search_dim;
    int deflation_grid;
    double tol_restart;

    int eigcg_max_restarts;
    int max_restart_num;
    double inc_tol;
    double eigenval_tol;

    QudaExtLibType   solver_ext_lib;
    QudaExtLibType   deflation_ext_lib;

    QudaFieldLocation location_ritz;
    QudaMemoryType    mem_type_ritz;

    char *vec_infile;
    char *vec_outfile;

  } QudaEigArgs_t;


  /**
   * Parameters related to problem size and machine topology.
   */
  typedef struct {
    const int* latsize; /** Local lattice dimensions */
    const int* machsize; /** Machine grid size */
    int device; /** GPU device  number */
  } QudaLayout_t;


  /**
   * Parameters used to create a QUDA context.
   */
  typedef struct {
    QudaVerbosity verbosity; /** How verbose QUDA should be (QUDA_SILENT, QUDA_VERBOSE or QUDA_SUMMARIZE) */
    QudaLayout_t layout; /** Layout for QUDA to use */
  } QudaInitArgs_t; // passed to the initialization struct


  /**
   * Parameters for defining HISQ calculations
   */
  typedef struct {
    int reunit_allow_svd;         /** Allow SVD for reuniarization */
    int reunit_svd_only;          /** Force use of SVD for reunitarization */
    double reunit_svd_abs_error;  /** Absolute error bound for SVD to apply */
    double reunit_svd_rel_error;  /** Relative error bound for SVD to apply */
    double force_filter;          /** UV filter to apply to force */
  } QudaHisqParams_t;


  /**
   * Parameters for defining fat-link calculations
   */
  typedef struct {
    int su3_source;          /** is the incoming gauge field SU(3) */
    int use_pinned_memory;   /** use page-locked memory in QUDA    */
  } QudaFatLinkArgs_t;

  /**
   * Parameters for propagator contractions with FT
   */
  typedef struct {
    int n_mom;                 /* Number of sink momenta */
    int *mom_modes;            /* List of 4-component momenta as integers. Dimension 4*n_mom */
    QudaFFTSymmType *fft_type; /* The "parity" of the FT component */
    int *source_position;      /* The coordinate origin for the Fourier phases */
    double flops;              /* Return value */
    double dtime;              /* Return value */
  } QudaContractArgs_t;

  /**
   * Parameters for two-link Gaussian quark smearing.
   */
  typedef struct {
    int n_steps; /** Number of steps to apply **/
    double width; /** The width of the Gaussian **/
    int compute_2link; /** if nonzero then compute two-link, otherwise reuse gaugeSmeared **/
    int delete_2link; /** if nonzero then delete two-link, otherwise keep two-link for future use **/
    int t0; /** Set if the input spinor is on a time slice **/
    int laplaceDim; /** Dimension of Laplacian **/
  } QudaTwoLinkQuarkSmearArgs_t;

  /**
   * Optional: Set the MPI Comm Handle if it is not MPI_COMM_WORLD
   *
   * @param[in] input Pointer to an MPI_Comm handle, static cast as a void *
   */
  void qudaSetMPICommHandle(void *mycomm);

  /**
   * Initialize the QUDA context.
   *
   * @param[in] input Meta data for the QUDA context
   */
  void qudaInit(QudaInitArgs_t input);

  /**
   * Set set the local dimensions and machine topology for QUDA to use
   *
   * @param[in] layout Struct defining local dimensions and machine topology
   */
  void qudaSetLayout(QudaLayout_t layout);

  /**
   * Clean up the QUDA deflation space.
   */
  void qudaCleanUpDeflationSpace();

  /**
   * Destroy the QUDA context.
   */
  void qudaFinalize();

  /**
   * Allocate pinned memory suitable for CPU-GPU transfers
   * @param[in] bytes The size of the requested allocation
   * @return Pointer to allocated memory
   */
  void* qudaAllocatePinned(size_t bytes);

  /**
   * Free pinned memory
   * @param[in] ptr Pointer to memory to be free
   */
  void qudaFreePinned(void *ptr);

  /**
   * Allocate managed memory to reduce CPU-GPU transfers
   * @param[in] bytes The size of the requested allocation
   * @return Pointer to allocated memory
   */
  void *qudaAllocateManaged(size_t bytes);

  /**
   * Free managed memory
   * @param[in] ptr Pointer to memory to be free
   */
  void qudaFreeManaged(void *ptr);

  /**
   * Set the algorithms to use for HISQ fermion calculations, e.g.,
   * SVD parameters for reunitarization.
   *
   * @param[in] hisq_params Meta data desribing the algorithms to use for HISQ fermions
   */
  void qudaHisqParamsInit(QudaHisqParams_t hisq_params);

  /**
   * Compute the fat and long links using the input gauge field.  All
   * fields passed here are host fields, that must be preallocated.
   * The precision of all fields must match.
   *
   * @param[in] precision The precision of the fields
   * @param[in] fatlink_args Meta data for the algorithms to deploy
   * @param[in] act_path_coeff Array of coefficients for each path in the action
   * @param[in] inlink Host gauge field used for input
   * @param[out] fatlink Host fat-link field that is computed
   * @param[out] longlink Host long-link field that is computed
   */
  void qudaLoadKSLink(int precision,
		      QudaFatLinkArgs_t fatlink_args,
		      const double act_path_coeff[6],
		      void* inlink,
		      void* fatlink,
		      void* longlink);

  /**
   * Compute the fat links and unitzarize using the input gauge field.
   * All fields passed here are host fields, that must be
   * preallocated.  The precision of all fields must match.
   *
   * @param[in] precision The precision of the fields
   * @param[in] fatlink_args Meta data for the algorithms to deploy
   * @param[in] path_coeff Array of coefficients for each path in the action
   * @param[in] inlink Host gauge field used for input
   * @param[out] fatlink Host fat-link field that is computed
   * @param[out] ulink Host unitarized field that is computed
   */
  void qudaLoadUnitarizedLink(int precision,
			      QudaFatLinkArgs_t fatlink_args,
			      const double path_coeff[6],
			      void* inlink,
			      void* fatlink,
			      void* ulink);

  /**
   * Apply the forward/backward/symmetric shift for the spin-taste opeartor. All fields
   * passed and returned are host (CPU) field in MILC order.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision     Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] links              Gauge field on the host
   * @param[in] src                Input spinor field
   * @param[out] dst                Output spinor field
   * @param[in] dir                Direction of application of the spin-taste operator
   * @param[in] sym                Kind of spin-taste operator (1 forward, 2 backward, 3 symmetric)
   * @param[in] reloadGaugeField   Should we transfer again the gauge field from the CPU to the GPU? (0 = false, anything else = true)
   */
  void qudaShift(int external_precision, int quda_precision, const void *const links, void *source, void *solution,
                 int dir, int sym, int reloadGaugeField);

  /**
   * Apply the forward/backward/symmetric shift for the spin-taste opeartor. All fields
   * passed and returned are host (CPU) field in MILC order.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision     Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] links              Gauge field on the host
   * @param[in] src                Input spinor field
   * @param[out] dst                Output spinor field
   * @param[in] spin               Spin gamma structure using MILC numbering
   * @param[in] taste              Taste gamma structure using MILC numbering
   * @param[in] reloadGaugeField   Should we transfer again the gauge field from the CPU to the GPU? (0 = false, anything else = true)
   */
  void qudaSpinTaste(int external_precision, int quda_precision, const void *const links, void *src, void *dst,
                     int spin, int taste, int reloadGaugeField);
  /**
   * Apply the improved staggered operator to a field. All fields
   * passed and returned are host (CPU) field in MILC order.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision     Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] inv_args           Struct setting some solver metadata
   * @param[in] milc_fatlink       Fat-link field on the host
   * @param[in] milc_longlink      Long-link field on the host
   * @param[in] source             Right-hand side source field
   * @param[out] solution           Solution spinor field
   */
  void qudaDslash(int external_precision,
		  int quda_precision,
		  QudaInvertArgs_t inv_args,
		  const void* const milc_fatlink,
		  const void* const milc_longlink,
		  void* source,
		  void* solution,
		  int* num_iters);

  /**
   * Solve Ax=b using an improved staggered operator with a
   * domain-decomposition preconditioner.  All fields are fields
   * passed and returned are host (CPU) field in MILC order.  This
   * function requires that persistent gauge and clover fields have
   * been created prior.  This interface is experimental.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] mass Fermion mass parameter
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] target_relative_residual Target Fermilab residual
   * @param[in] domain_overlap Array specifying the overlap of the domains in each dimension
   * @param[in] fatlink Fat-link field on the host
   * @param[in] longlink Long-link field on the host
   * @param[in] source Right-hand side source field
   * @param[out] solution Solution spinor field
   * @param[in] final_residual True residual
   * @param[in] final_relative_residual True Fermilab residual
   * @param[in] num_iters Number of iterations taken
   */
  void qudaDDInvert(int external_precision,
		    int quda_precision,
		    double mass,
		    QudaInvertArgs_t inv_args,
		    double target_residual,
		    double target_fermilab_residual,
		    const int * const domain_overlap,
		    const void* const fatlink,
		    const void* const longlink,
		    void* source,
		    void* solution,
		    double* const final_residual,
		    double* const final_fermilab_residual,
		    int* num_iters);

  /**
   * Solve Ax=b for an improved staggered operator. All fields are fields
   * passed and returned are host (CPU) field in MILC order.  This
   * function requires that persistent gauge and clover fields have
   * been created prior.  This interface is experimental.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] mass Fermion mass parameter
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] target_relative_residual Target Fermilab residual
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] source Right-hand side source field
   * @param[out] solution Solution spinor field
   * @param[in] final_residual True residual
   * @param[in] final_relative_residual True Fermilab residual
   * @param[in] num_iters Number of iterations taken
   */
  void qudaInvert(int external_precision,
		  int quda_precision,
		  double mass,
		  QudaInvertArgs_t inv_args,
		  double target_residual,
		  double target_fermilab_residual,
		  const void* const milc_fatlink,
		  const void* const milc_longlink,
		  void* source,
		  void* solution,
		  double* const final_resid,
		  double* const final_rel_resid,
		  int* num_iters);

  /**
   * Prepare a staggered/HISQ multigrid solve with given fat and
   * long links. All fields passed are host (CPU) fields
   * in MILC order. This function requires persistent gauge fields.
   * This interface is experimental.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] mass Fermion mass parameter
   * @param[in] inv_args Struct setting some solver metadata; required for tadpole, naik coeff
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] mg_param_file Path to an input text file describing the MG solve, to be documented on QUDA wiki
   * @return Void pointer wrapping a pack of multigrid-related structures
   */
  void *qudaMultigridCreate(int external_precision, int quda_precision, double mass, QudaInvertArgs_t inv_args,
                            const void *const milc_fatlink, const void *const milc_longlink,
                            const char *const mg_param_file);

  /**
   * Solve Ax=b for an improved staggered operator using MG.
   * All fields are fields passed and returned are host (CPU)
   * field in MILC order.  This function requires that persistent
   * gauge and clover fields have been created prior. It also
   * requires a multigrid parameter built from qudaSetupMultigrid
   * This interface is experimental.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] mass Fermion mass parameter
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] target_relative_residual Target Fermilab residual
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] mg_pack_ptr MG preconditioner structure created by qudaSetupMultigrid
   * @param[in] mg_rebuild_type whether to do a full (1) or thin (0) MG rebuild
   * @param[in] source Right-hand side source field
   * @param[out] solution Solution spinor field
   * @param[in] final_residual True residual
   * @param[in] final_relative_residual True Fermilab residual
   * @param[in] num_iters Number of iterations taken
   */
  void qudaInvertMG(int external_precision, int quda_precision, double mass, QudaInvertArgs_t inv_args,
                    double target_residual, double target_fermilab_residual, const void *const milc_fatlink,
                    const void *const milc_longlink, void *mg_pack_ptr, int mg_rebuild_type, void *source,
                    void *solution, double *const final_residual, double *const final_fermilab_residual, int *num_iters);

  /**
   * Clean up a staggered/HISQ multigrid object, freeing all internal
   * fields and otherwise allocated memory.
   *
   * @param[in] mg_pack_ptr Void pointer mapping to the multigrid structure returned by qudaSetupMultigrid
   */
  void qudaMultigridDestroy(void *mg_pack_ptr);

  /**
   * Solve Ax=b for an improved staggered operator with many right hand sides.
   * All fields are fields passed and returned are host (CPU) field in MILC order.
   * This function requires that persistent gauge and clover fields have
   * been created prior.  This interface is experimental.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] mass Fermion mass parameter
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] target_relative_residual Target Fermilab residual
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] source array of right-hand side source fields
   * @param[out] solution array of solution spinor fields
   * @param[in] final_residual True residual
   * @param[in] final_relative_residual True Fermilab residual
   * @param[in] num_iters Number of iterations taken
   * @param[in] num_src Number of source fields
   */
  void qudaInvertMsrc(int external_precision,
                      int quda_precision,
                      double mass,
                      QudaInvertArgs_t inv_args,
                      double target_residual,
                      double target_fermilab_residual,
                      const void* const fatlink,
                      const void* const longlink,
                      void** sourceArray,
                      void** solutionArray,
                      double* const final_residual,
                      double* const final_fermilab_residual,
                      int* num_iters,
                      int num_src);

  /**
   * Solve for multiple shifts (e.g., masses) using an improved
   * staggered operator.  All fields are fields passed and returned
   * are host (CPU) field in MILC order.  This function requires that
   * persistent gauge and clover fields have been created prior.  When
   * a pure double-precision solver is requested no reliable updates
   * are used, else reliable updates are used with a reliable_delta
   * parameter of 0.1.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] num_offsets Number of shifts to solve for
   * @param[in] offset Array of shift offset values
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Array of target residuals per shift
   * @param[in] target_relative_residual Array of target Fermilab residuals per shift
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] source Right-hand side source field
   * @param[out] solutionArray Array of solution spinor fields
   * @param[in] final_residual Array of true residuals
   * @param[in] final_relative_residual Array of true Fermilab residuals
   * @param[in] num_iters Number of iterations taken
   */
  void qudaMultishiftInvert(
      int external_precision,
      int precision,
      int num_offsets,
      double* const offset,
      QudaInvertArgs_t inv_args,
      const double* target_residual,
      const double* target_fermilab_residual,
      const void* const milc_fatlink,
      const void* const milc_longlink,
      void* source,
      void** solutionArray,
      double* const final_residual,
      double* const final_fermilab_residual,
      int* num_iters);

  /**
   * Solve for a system with many RHS using an improved
   * staggered operator.
   * The solving procedure consists of two computation phases :
   * 1) incremental pahse : call eigCG solver to accumulate low eigenmodes
   * 2) deflation phase : use computed eigenmodes to deflate a regular CG
   * All fields are fields passed and returned
   * are host (CPU) field in MILC order.  This function requires that
   * persistent gauge and clover fields have been created prior.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] num_offsets Number of shifts to solve for
   * @param[in] offset Array of shift offset values
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Array of target residuals per shift
   * @param[in] target_relative_residual Array of target Fermilab residuals per shift
   * @param[in] milc_fatlink Fat-link field on the host
   * @param[in] milc_longlink Long-link field on the host
   * @param[in] source Right-hand side source field
   * @param[out] solution Array of solution spinor fields
   * @param[in] eig_args contains info about deflation space
   * @param[in] rhs_idx  bookkeep current rhs
   * @param[in] last_rhs_flag  is this the last rhs to solve?
   * @param[in] final_residual Array of true residuals
   * @param[in] final_relative_residual Array of true Fermilab residuals
   * @param[in] num_iters Number of iterations taken
   */
  void qudaEigCGInvert(
      int external_precision,
      int quda_precision,
      double mass,
      QudaInvertArgs_t inv_args,
      double target_residual,
      double target_fermilab_residual,
      const void* const fatlink,
      const void* const longlink,
      void* source,
      void* solution,
      QudaEigArgs_t eig_args,
      const int rhs_idx,//current rhs
      const int last_rhs_flag,//is this the last rhs to solve?
      double* const final_residual,
      double* const final_fermilab_residual,
      int *num_iters);

  /**
   * Solve Ax=b using a Wilson-Clover operator.  All fields are fields
   * passed and returned are host (CPU) field in MILC order.  This
   * function creates the gauge and clover field from the host fields.
   * Reliable updates are used with a reliable_delta parameter of 0.1.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] kappa Kappa value
   * @param[in] clover_coeff Clover coefficient
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] milc_link Gauge field on the host
   * @param[in] milc_clover Clover field on the host
   * @param[in] milc_clover_inv Inverse clover on the host
   * @param[in] clover_coeff Clover coefficient
   * @param[in] source Right-hand side source field
   * @param[out] solution Solution spinor field
   * @param[in] final_residual True residual returned by the solver
   * @param[in] final_residual True Fermilab residual returned by the solver
   * @param[in] num_iters Number of iterations taken
   */
  void qudaCloverInvert(int external_precision,
			int quda_precision,
			double kappa,
			double clover_coeff,
			QudaInvertArgs_t inv_args,
			double target_residual,
			double target_fermilab_residual,
			const void* milc_link,
			void* milc_clover,
			void* milc_clover_inv,
			void* source,
			void* solution,
			double* const final_residual,
			double* const final_fermilab_residual,
			int* num_iters);

  /**
   * Solve for a system with many RHS using using a Wilson-Clover operator.
   * The solving procedure consists of two computation phases :
   * 1) incremental pahse : call eigCG solver to accumulate low eigenmodes
   * 2) deflation phase : use computed eigenmodes to deflate a regular CG
   * All fields are fields passed and returned
   * are host (CPU) field in MILC order.  This function requires that
   * persistent gauge and clover fields have been created prior.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] kappa Kappa value
   * @param[in] clover_coeff Clover coefficient
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Target residual
   * @param[in] milc_link Gauge field on the host
   * @param[in] milc_clover Clover field on the host
   * @param[in] milc_clover_inv Inverse clover on the host
   * @param[in] clover_coeff Clover coefficient
   * @param[in] source Right-hand side source field
   * @param[out] solution Solution spinor field
   * @param[in] eig_args contains info about deflation space
   * @param[in] rhs_idx  bookkeep current rhs
   * @param[in] last_rhs_flag  is this the last rhs to solve?
   * @param[in] final_residual Array of true residuals
   * @param[in] final_relative_residual Array of true Fermilab residuals
   * @param[in] num_iters Number of iterations taken
   */
  void qudaEigCGCloverInvert(
      int external_precision,
      int quda_precision,
      double kappa,
      double clover_coeff,
      QudaInvertArgs_t inv_args,
      double target_residual,
      double target_fermilab_residual,
      const void* milc_link,
      void* milc_clover,
      void* milc_clover_inv,
      void* source,
      void* solution,
      QudaEigArgs_t eig_args,
      const int rhs_idx,//current rhs
      const int last_rhs_flag,//is this the last rhs to solve?
      double* const final_residual,
      double* const final_fermilab_residual,
      int *num_iters);

  /**
   * Load the gauge field from the host.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] inv_args Meta data
   * @param[in] milc_link Base pointer to host gauge field (regardless of dimensionality)
   */
  void qudaLoadGaugeField(int external_precision,
			  int quda_precision,
			  QudaInvertArgs_t inv_args,
			  const void* milc_link) ;

  /**
     Free the gauge field allocated in QUDA.
   */
  void qudaFreeGaugeField();


  /**.     
     Free the two-link field allocated in QUDA.
   */
  void qudaFreeTwoLink();

  /**
   * Load the clover field and its inverse from the host.  If null
   * pointers are passed, the clover field and / or its inverse will
   * be computed dynamically from the resident gauge field.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] inv_args Meta data
   * @param[in] milc_clover Pointer to host clover field.  If 0 then the
   * clover field is computed dynamically within QUDA.
   * @param[in] milc_clover_inv Pointer to host inverse clover field.  If
   * 0 then the inverse if computed dynamically within QUDA.
   * @param[in] solution_type The type of solution required  (mat, matpc)
   * @param[in] solve_type The solve type to use (normal/direct/preconditioning)
   * @param[in] clover_coeff Clover coefficient
   * @param[in] compute_trlog Whether to compute the trlog of the clover field when inverting
   * @param[in] Array for storing the trlog (length two, one for each parity)
   */
  void qudaLoadCloverField(int external_precision,
			   int quda_precision,
			   QudaInvertArgs_t inv_args,
			   void* milc_clover,
			   void* milc_clover_inv,
			   QudaSolutionType solution_type,
			   QudaSolveType solve_type,
			   double clover_coeff,
			   int compute_trlog,
			   double *trlog) ;

  /**
     Free the clover field allocated in QUDA.
   */
  void qudaFreeCloverField();

  /**
   * Solve for multiple shifts (e.g., masses) using a Wilson-Clover
   * operator with multi-shift CG.  All fields are fields passed and
   * returned are host (CPU) field in MILC order.  This function
   * requires that persistent gauge and clover fields have been
   * created prior.  When a pure double-precision solver is requested
   * no reliable updates are used, else reliable updates are used with
   * a reliable_delta parameter of 0.1.
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] num_offsets Number of shifts to solve for
   * @param[in] offset Array of shift offset values
   * @param[in] kappa Kappa value
   * @param[in] clover_coeff Clover coefficient
   * @param[in] inv_args Struct setting some solver metadata
   * @param[in] target_residual Array of target residuals per shift
   * @param[in] clover_coeff Clover coefficient
   * @param[in] source Right-hand side source field
   * @param[out] solutionArray Array of solution spinor fields
   * @param[in] final_residual Array of true residuals
   * @param[in] num_iters Number of iterations taken
   */
  void qudaCloverMultishiftInvert(int external_precision,
      int quda_precision,
      int num_offsets,
      double* const offset,
      double kappa,
      double clover_coeff,
      QudaInvertArgs_t inv_args,
      const double* target_residual,
      void* source,
      void** solutionArray,
      double* const final_residual,
      int* num_iters
      );

  /**
   * Compute the fermion force for the HISQ quark action.  All fields
   * are host fields in MILC order, and the precision of these fields
   * must match.
   *
   * @param[in] precision       The precision of the fields
   * @param[in] num_terms The number of quark fields
   * @param[in] num_naik_terms The number of naik contributions
   * @param[in] dt Integrating step size
   * @param[in] coeff The coefficients multiplying the fermion fields in the outer product
   * @param[in] quark_field The input fermion field.
   * @param[in] level2_coeff    The coefficients for the second level of smearing in the quark action.
   * @param[in] fat7_coeff      The coefficients for the first level of smearing (fat7) in the quark action.
   * @param[in] w_link          Unitarized link variables obtained by applying fat7 smearing and unitarization to the
   * original links.
   * @param[in] v_link          Fat7 link variables.
   * @param[in] u_link          SU(3) think link variables.
   * @param[in] milc_momentum        The momentum contribution from the quark action.
   */
  void qudaHisqForce(int precision,
                     int num_terms,
                     int num_naik_terms,
                     double dt,
                     double** coeff,
                     void** quark_field,
		     const double level2_coeff[6],
		     const double fat7_coeff[6],
		     const void* const w_link,
		     const void* const v_link,
		     const void* const u_link,
		     void* const milc_momentum);

  /**
   * Compute the gauge force and update the momentum field.  All fields
   * here are CPU fields in MILC order, and their precisions should
   * match.
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[in] num_loop_types 1, 2 or 3
   * @param[in] milc_loop_coeff Coefficients of the different loops in the Symanzik action
   * @param[in] eb3 The integration step size (for MILC this is dt*beta/3)
   * @param[in] arg Metadata for MILC's internal site struct array
   */
  void qudaGaugeForce(int precision,
		      int num_loop_types,
		      double milc_loop_coeff[3],
		      double eb3,
		      QudaMILCSiteArg_t *arg);

  /**
   * Compute the gauge force and update the momentum field.  All fields
   * here are CPU fields in MILC order, and their precisions should
   * match.
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[in] num_loop_types 1, 2 or 3
   * @param[in] milc_loop_coeff Coefficients of the different loops in the Symanzik action
   * @param[in] eb3 The integration step size (for MILC this is dt*beta/3)
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaGaugeForcePhased(int precision, int num_loop_types, double milc_loop_coeff[3], double eb3,
                            QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Compute the real traces of gauge loops, with direct application to computing the gauge
   * action.  All fields here are CPU fields in MILC order, and their precisions should
   * match.
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[out] traces A pre-allocated buffer for computed traces of length 2 x num_paths to encode real and imaginary
   * @param[in] input_path_buf A double pointer of length num_paths x max_length containing loop paths
   * @param[in] path_length An array of length num_paths containing the lengths of each loop
   * @param[in] loop_coeff Coefficients for each individual loop
   * @param[in] num_paths The total number of paths that are computed
   * @param[in] max_length The maximum length across all loop paths
   * @param[in] factor An overall multiplicative factor applied to all traces
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaGaugeLoopTracePhased(int precision, double *traces, int **input_path_buf, int *path_length, double *loop_coeff,
                                int num_paths, int max_length, double factor, QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Compute the total, spatial, and temporal plaquette. All fields here are CPU fields in
   * MILC order, and their precisions should match
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[out] plaq Storage for the total, spatial, and temporal plaquette
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaPlaquettePhased(int precision, double plaq[3], QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Compute the real and imaginary parts of the Polyakov loop in a given direction. All fields here are
   * CPU fields in MILC order, and their precisions should match
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[out] ploop Storage for the output Polyakov loop
   * @param[in] dir Direction of the Polyakov loop (0 - x, 1 - y, 2 - z, 3 - t)
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaPolyakovLoopPhased(int precision, double ploop[2], int dir, QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Compute the plaquette, temporal Polyakov loop, and real traces of gauge loops in one go. The
   * gauge loop traces have a direct application to computing the gauge action. All fields here
   * are CPU fields in MILC order, and their precisions should match.
   *
   * @param[in] precision The precision of the field (2 - double, 1 - single)
   * @param[out] plaq Storage for the total, spatial, and temporal plaquette
   * @param[out] ploop Storage for the output Polyakov loop
   * @param[in] Direction of the Polyakov loop (0 - x, 1 - y, 2 - z, 3 - t)
   * @param[out] traces A pre-allocated buffer for computed traces of length 2 x num_paths to encode real and imaginary
   * @param[in] input_path_buf A double pointer of length num_paths x max_length containing loop paths
   * @param[in] path_length An array of length num_paths containing the lengths of each loop
   * @param[in] loop_coeff Coefficients for each individual loop
   * @param[in] num_paths The total number of paths that are computed
   * @param[in] max_length The maximum length across all loop paths
   * @param[in] factor An overall multiplicative factor applied to all traces
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaGaugeMeasurementsPhased(int precision, double plaq[3], double ploop[2], int dir, double *traces,
                                   int **input_path_buf, int *path_length, double *loop_coeff, int num_paths,
                                   int max_length, double factor, QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Evolve the gauge field by step size dt, using the momentum field
   * I.e., Evalulate U(t+dt) = e(dt pi) U(t).  All fields are CPU fields in MILC order.
   *
   * @param[in] precision Precision of the field (2 - double, 1 - single)
   * @param[in] dt The integration step size step
   * @param[in] arg Metadata for MILC's internal site struct array
   */
  void qudaUpdateU(int precision,
		   double eps,
		   QudaMILCSiteArg_t *arg);

  /**
   * Evolve the gauge field by step size dt, using the momentum field
   * I.e., Evalulate U(t+dt) = e(dt pi) U(t).  All fields are CPU fields in MILC order.
   *
   * @param[in] precision Precision of the field (2 - double, 1 - single)
   * @param[in] dt The integration step size step
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaUpdateUPhased(int precision, double eps, QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Evolve the gauge field by step size dt, using the momentum field
   * I.e., Evalulate U(t+dt) = e(dt pi) U(t).  All fields are CPU fields in MILC order.
   *
   * @param[in] precision Precision of the field (2 - double, 1 - single)
   * @param[in] dt The integration step size step
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   * @param[in] want_gaugepipe whether to enabled QUDA gaugepipe for HMC
   */
  void qudaUpdateUPhasedPipeline(int precision, double eps, QudaMILCSiteArg_t *arg, int phase_in, int want_gaugepipe);

  /**
   * Download the momentum from MILC and place into QUDA's resident
   * momentum field.  The source momentum field can either be as part
   * of a MILC site struct (QUDA_MILC_SITE_GAUGE_ORDER) or as a
   * separate field (QUDA_MILC_GAUGE_ORDER).
   *
   * @param precision Precision of the field (2 - double, 1 - single)
   * @param arg Metadata for MILC's internal site struct array
   */
  void qudaMomLoad(int precision, QudaMILCSiteArg_t *arg);

  /**
   * Upload the momentum to MILC from QUDA's resident momentum field.
   * The destination momentum field can either be as part of a MILC site
   * struct (QUDA_MILC_SITE_GAUGE_ORDER) or as a separate field
   * (QUDA_MILC_GAUGE_ORDER).
   *
   * @param[in] precision Precision of the field (2 - double, 1 - single)
   * @param[in] arg Metadata for MILC's internal site struct array
   */
  void qudaMomSave(int precision, QudaMILCSiteArg_t *arg);

  /**
   * Evaluate the momentum contribution to the Hybrid Monte Carlo
   * action.  MILC convention is applied, subtracting 4.0 from each
   * momentum matrix to increase stability.
   *
   * @param[in] precision Precision of the field (2 - double, 1 - single)
   * @param[in] arg Metadata for MILC's internal site struct array
   * @return momentum action
   */
  double qudaMomAction(int precision, QudaMILCSiteArg_t *arg);

  /**
   * Apply the staggered phase factors to the gauge field.  If the
   * imaginary chemical potential is non-zero then the phase factor
   * exp(imu/T) will be applied to the links in the temporal
   * direction.
   *
   * @param[in] prec Precision of the gauge field
   * @param[in/out] gauge_h The gauge field
   * @param[in] flag Whether to apply to remove the staggered phase
   * @param[in] i_mu Imaginary chemical potential
   */
  void qudaRephase(int prec, void *gauge, int flag, double i_mu);

  /**
   * Project the input field on the SU(3) group.  If the target
   * tolerance is not met, this routine will give a runtime error.
   *
   * @param[in] prec Precision of the gauge field
   * @param[in] tol The tolerance to which we iterate
   * @param[in] arg Metadata for MILC's internal site struct array
   */
  void qudaUnitarizeSU3(int prec, double tol, QudaMILCSiteArg_t *arg);

  /**
   * Project the input field on the SU(3) group.  If the target
   * tolerance is not met, this routine will give a runtime error.
   *
   * @param[in] prec Precision of the gauge field
   * @param[in] tol The tolerance to which we iterate
   * @param[in] arg Metadata for MILC's internal site struct array
   * @param[in] phase_in whether staggered phases are applied
   */
  void qudaUnitarizeSU3Phased(int prec, double tol, QudaMILCSiteArg_t *arg, int phase_in);

  /**
   * Compute the clover force contributions in each dimension mu given
   * the array solution fields, and compute the resulting momentum
   * field.
   *
   * @param[in] mom Momentum matrix
   * @param[in] dt Integrating step size
   * @param[out] x Array of solution vectors
   * @param[in] p Array of intermediate vectors
   * @param[in] coeff Array of residues for each contribution
   * @param[in] kappa kappa parameter
   * @param[in] ck -clover_coefficient * kappa / 8
   * @param[in] nvec Number of vectors
   * @param[in] multiplicity Number of fermions represented by this bilinear
   * @param[in] gauge Gauge Field
   * @param[in] precision Precision of the fields
   * @param[in] inv_args Struct setting some solver metadata
   */
  void qudaCloverForce(void *mom, double dt, void **x, void **p, double *coeff, double kappa,
		       double ck, int nvec, double multiplicity, void *gauge, int precision,
		       QudaInvertArgs_t inv_args);

  /**
   * Compute the sigma trace field (part of clover force computation).
   * All the pointers here are for QUDA native device objects.  The
   * precisions of all fields must match.  This function requires that
   * there is a persistent clover field.
   *
   * @param[out] out Sigma trace field  (QUDA device field, geometry = 1)
   * @param[in] dummy (not used)
   * @param[in] mu mu direction
   * @param[in] nu nu direction
   */
  void qudaCloverTrace(void* out,
		       void* dummy,
		       int mu,
		       int nu);

  /**
   * Compute the derivative of the clover term (part of clover force
   * computation).  All the pointers here are for QUDA native device
   * objects.  The precisions of all fields must match.
   *
   * @param[out] out Clover derivative field (QUDA device field, geometry = 1)
   * @param[in] gauge Gauge field (extended QUDA device field, gemoetry = 4)
   * @param[in] oprod Matrix field (outer product) which is multiplied by the derivative
   * @param[in] mu mu direction
   * @param[in] nu nu direction
   * @param[in] coeff Coefficient of the clover derviative (including stepsize and clover coefficient)
   * @param[in] precision Precision of the fields (2 = double, 1 = single)
   * @param[in] parity Parity for which we are computing
   * @param[in] conjugate Whether to make the oprod field anti-hermitian prior to multiplication
   */
  void qudaCloverDerivative(void* out,
			    void* gauge,
			    void* oprod,
			    int mu,
			    int nu,
			    double coeff,
			    int precision,
			    int parity,
			    int conjugate);

  /**
   * Take a gauge field on the host, load it onto the device and extend it.
   * Return a pointer to the extended gauge field object.
   *
   * @param[in] gauge The CPU gauge field (optional - if set to 0 then the gauge field zeroed)
   * @param[in] geometry The geometry of the matrix field to create (1 - scaler, 4 - vector, 6 - tensor)
   * @param[in] precision The precision of the fields (2 - double, 1 - single)
   * @return Pointer to the gauge field (cast as a void*)
   */
  void* qudaCreateExtendedGaugeField(void* gauge,
				     int geometry,
				     int precision);

  /**
   * Take the QUDA resident gauge field and extend it.
   * Return a pointer to the extended gauge field object.
   *
   * @param[in] gauge The CPU gauge field (optional - if set to 0 then the gauge field zeroed)
   * @param[in] geometry The geometry of the matrix field to create (1 - scaler, 4 - vector, 6 - tensor)
   * @param[in] precision The precision of the fields (2 - double, 1 - single)
   * @return Pointer to the gauge field (cast as a void*)
   */
  void* qudaResidentExtendedGaugeField(void* gauge,
				       int geometry,
				       int precision);

  /**
   * Allocate a gauge (matrix) field on the device and optionally download a host gauge field.
   *
   * @param[in] gauge The host gauge field (optional - if set to 0 then the gauge field zeroed)
   * @param[in] geometry The geometry of the matrix field to create (1 - scaler, 4 - vector, 6 - tensor)
   * @param[in] precision The precision of the field to be created (2 - double, 1 - single)
   * @return Pointer to the gauge field (cast as a void*)
   */
  void* qudaCreateGaugeField(void* gauge,
			     int geometry,
			     int precision);

  /**
   * Copy the QUDA gauge (matrix) field on the device to the CPU
   *
   * @param[out] outGauge Pointer to the host gauge field
   * @param[in] inGauge Pointer to the device gauge field (QUDA device field)
   */
  void qudaSaveGaugeField(void* gauge,
			  void* inGauge);

  /**
   * Reinterpret gauge as a pointer to a GaugeField and call destructor.
   *
   * @param[in] gauge Gauge field to be freed
   */
  void qudaDestroyGaugeField(void* gauge);

  /**
   * @brief Gauge fixing with overrelaxation with support for single and multi GPU.
   * @param[in] precision, 1 for single precision else for double precision
   * @param[in] gauge_dir, 3 for Coulomb gauge fixing, other for Landau gauge fixing
   * @param[in] Nsteps, maximum number of steps to perform gauge fixing
   * @param[in] verbose_interval, print gauge fixing info when iteration count is a multiple of this
   * @param[in] relax_boost, gauge fixing parameter of the overrelaxation method, most common value is 1.5 or 1.7.
   * @param[in] tolerance, torelance value to stop the method, if this value is zero then the method stops when
   * iteration reachs the maximum number of steps defined by Nsteps
   * @param[in] reunit_interval, reunitarize gauge field when iteration count is a multiple of this
   * @param[in] stopWtheta, 0 for MILC criterion and 1 to use the theta value
   * @param[in,out] milc_sitelink, MILC gauge field to be fixed
   */
  void qudaGaugeFixingOVR(int precision, unsigned int gauge_dir, int Nsteps, int verbose_interval, double relax_boost,
                          double tolerance, unsigned int reunit_interval, unsigned int stopWtheta,
                          QudaMILCSiteArg_t *arg);

  /**
   * @brief Gauge fixing with Steepest descent method with FFTs with support for single GPU only.
   * @param[in] precision, 1 for single precision else for double precision
   * @param[in] gauge_dir, 3 for Coulomb gauge fixing, other for Landau gauge fixing
   * @param[in] Nsteps, maximum number of steps to perform gauge fixing
   * @param[in] verbose_interval, print gauge fixing info when iteration count is a multiple of this
   * @param[in] alpha, gauge fixing parameter of the method, most common value is 0.08
   * @param[in] autotune, 1 to autotune the method, i.e., if the Fg inverts its tendency we decrease the alpha value
   * @param[in] tolerance, torelance value to stop the method, if this value is zero then the method stops when
   * iteration reachs the maximum number of steps defined by Nsteps
   * @param[in] stopWtheta, 0 for MILC criterion and 1 to use the theta value
   * @param[in,out] milc_sitelink, MILC gauge field to be fixed
   */
  void qudaGaugeFixingFFT( int precision,
    unsigned int gauge_dir,
    int Nsteps,
    int verbose_interval,
    double alpha,
    unsigned int autotune,
    double tolerance,
    unsigned int stopWtheta,
    void* milc_sitelink
    );

  /**
   * @brief Tie together two staggered propagators including spatial Fourier phases.
   * The result is summed separately over each time slice and across all MPI ranks.
   * The FT is defined by a list of momentum indices (three-component integer vectors)
   * Included with the FT is a parity (symmetry) parameter for each momentum
   * component that selects an exp, cos, or sin factor for each direction
   *
   * @param[in] external_precision Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in,out] parameters for the contraction, including FT specification
   * @param[in] local storage of color spinor field.  three complex values * number of sites on node
   * @param[in] local storage of color spinor field.  three complex values * number of sites on node
   * @param[out] hadron correlator  Flattened double array as though [n_mom][nt][2] for 2 = re,im.
   */
  void qudaContractFT(int external_precision, QudaContractArgs_t *cont_args, void *const quark1, void *const quark2,
                      double *corr);

  /**
   * @brief Perform two-link Gaussian smearing on a given spinor (for staggered fermions).
   * @param[in] external_precision  Precision of host fields passed to QUDA (2 - double, 1 - single)
   * @param[in] quda_precision  Precision for QUDA to use (2 - double, 1 - single)
   * @param[in] h_gauge  Host gauge field
   * @param[in,out] source  Spinor field to smear
   * @param[in] qsmear_args  Struct setting some smearing metadata
   */
  void qudaTwoLinkGaussianSmear(int external_precision, int quda_precision, void * h_gauge, void * source,
                                QudaTwoLinkQuarkSmearArgs_t qsmear_args);

  /* The below declarations are for removed functions from prior versions of QUDA. */

  /**
   * Note this interface function has been removed.  This stub remains
   * for compatibility only.
   */
  void qudaAsqtadForce(int precision,
		       const double act_path_coeff[6],
		       const void* const one_link_src[4],
		       const void* const naik_src[4],
		       const void* const link,
		       void* const milc_momentum);

  /**
   * Note this interface function has been removed.  This stub remains
   * for compatibility only.
   */
  void qudaComputeOprod(int precision,
			int num_terms,
			int num_naik_terms,
			double** coeff,
                        double scale,
			void** quark_field,
			void* oprod[3]);

#ifdef __cplusplus
}
#endif


#endif // _QUDA_MILC_INTERFACE_H
