#pragma once

#include <quda_matrix.h>
#include <index_helper.cuh>
#include <gauge_field_order.h>
#include <kernel.h>

namespace quda {

  namespace fermion_force {

    enum {
      XUP = 0,
      YUP = 1,
      ZUP = 2,
      TUP = 3,
      TDOWN = 4,
      ZDOWN = 5,
      YDOWN = 6,
      XDOWN = 7
    };

    constexpr int opp_dir(int dir) { return 7-dir; }
    constexpr int goes_forward(int dir) { return dir<=3; }
    constexpr int goes_backward(int dir) { return dir>3; }
    constexpr int CoeffSign(int pos_dir, int odd_lattice) { return 2*((pos_dir + odd_lattice + 1) & 1) - 1; }
    constexpr int Sign(int parity) { return parity ? -1 : 1; }
    constexpr int posDir(int dir) { return (dir >= 4) ? 7-dir : dir; }

    constexpr int updateCoordsIndex(int x[], const int X[], int dir, int shift) {
      switch (dir) {
      case 0: x[0] = (x[0] + shift + X[0]) % X[0]; break;
      case 1: x[1] = (x[1] + shift + X[1]) % X[1]; break;
      case 2: x[2] = (x[2] + shift + X[2]) % X[2]; break;
      case 3: x[3] = (x[3] + shift + X[3]) % X[3]; break;
      }
      int idx = (((x[3] * X[2] + x[2]) * X[1] + x[1]) * X[0] + x[0]) >> 1;
      return idx;
    }

    //struct for holding the fattening path coefficients
    template <typename real>
    struct PathCoefficients {
      const real one;
      const real naik;
      const real three;
      const real five;
      const real seven;
      const real lepage;
      PathCoefficients(const double *path_coeff_array)
        : one(path_coeff_array[0]), naik(path_coeff_array[1]),
          three(path_coeff_array[2]), five(path_coeff_array[3]),
          seven(path_coeff_array[4]), lepage(path_coeff_array[5]) { }
    };

    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct BaseForceArg : kernel_param<> {
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      const Gauge link;
      int X[4]; // regular grid dims
      int D[4]; // working set grid dims
      int E[4]; // extended grid dims

      int commDim[4];
      int border[4];
      int base_idx[4]; // the offset into the extended field
      int oddness_change;

      // for readability, we explicitly set the different directions
      int mu;
      int nu;
      int rho;
      int sig;

      /**
         @param[in] link Gauge field
         @param[in] overlap Radius of additional redundant computation to do
       */
      BaseForceArg(const GaugeField &link, int overlap) :
        kernel_param(dim3(1, 2, 1)),
        link(link),
        commDim{ comm_dim_partitioned(0), comm_dim_partitioned(1), comm_dim_partitioned(2), comm_dim_partitioned(3) },
        mu(-1), nu(-1), rho(-1), sig(-1)
      {
        for (int d=0; d<4; d++) {
          E[d] = link.X()[d];
          border[d] = link.R()[d];
          X[d] = E[d] - 2*border[d];
          D[d] = comm_dim_partitioned(d) ? X[d]+overlap*2 : X[d];
          base_idx[d] = comm_dim_partitioned(d) ? border[d]-overlap : 0;
          this->threads.x *= D[d];
        }
        this->threads.x /= 2;
        oddness_change = (base_idx[0] + base_idx[1] + base_idx[2] + base_idx[3])&1;
      }
    };

    template <typename Arg_, int orthogonal_positive_ = 0, int sig_positive_ = 0>
    struct FatLinkParam : kernel_param<> {
      // whether or not the "orthogonal" direction is forwards or backwards
      static constexpr int orthogonal_positive = orthogonal_positive_;
      // whether the base link direction is forwards or backwards
      static constexpr int sig_positive = sig_positive_;

      // base argument structure
      using Arg = Arg_;
      Arg arg;
      FatLinkParam(Arg &arg) :
        kernel_param<>(arg.threads),
        arg(arg) {}
    };

    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct OneLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;

      const Gauge oProd;
      const real coeff;

      static constexpr int overlap = 0;

      OneLinkArg(GaugeField &force, const GaugeField &oProd, const GaugeField &link, real coeff)
        : BaseForceArg(link, overlap), force(force), oProd(oProd), coeff(coeff) {
          this->threads.z = 4;
        }

    };

    template <typename Arg> struct OneLinkTerm
    {
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      constexpr OneLinkTerm(const Arg &arg) : arg(arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity, int sig)
      {
        int x[4];
        getCoords(x, x_cb, arg.X, parity);
#pragma unroll
        for (int d=0; d<4; d++) x[d] += arg.border[d];
        int e_cb = linkIndex(x,arg.E);

        Link w = arg.oProd(sig, e_cb, parity);
        Link force = arg.force(sig, e_cb, parity);
        force += arg.coeff * w;
        arg.force(sig, e_cb, parity) = force;
      }
    };

    /**************************middleThreeLinkKernel*****************************
     *
     *
     * Generally we need
     * READ
     *    3 LINKS:         ab_link,     bc_link,    ad_link
     *    2 COLOR MATRIX:  newOprod_at_A, oprod_at_C
     * WRITE
     *    4 COLOR MATRIX:  newOprod_at_A, P3_at_A, Pmu_at_B, Qmu_at_A
     *
     *   In all three above case, if the direction sig is negative, newOprod_at_A is
     *   not read in or written out.
     *
     * Therefore the data traffic, in two-number pair (num_of_link, num_of_color_matrix)
     *   Call 1:  (called 48 times, half positive sig, half negative sig)
     *             if (sig is positive):    (3, 6)
     *             else               :     (3, 4)
     *
     * note: oprod_at_C could actually be read in from D when it is the fresh outer product
     *       and we call it oprod_at_C to simply naming. This does not affect our data traffic analysis
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *   call 1:     if (sig is positive)  (3, 1)
     *               else                  (2, 0)
     *
     ****************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct MiddleThreeLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge pMu;
      Gauge p3;
      Gauge qMu;

      const Gauge oProd;
      const real coeff;

      static constexpr int overlap = 2;

      MiddleThreeLinkArg(GaugeField &force, GaugeField &pMu, GaugeField &P3, GaugeField &qMu,
                 const GaugeField &oProd, const GaugeField &link,
                 real coeff)
        : BaseForceArg(link, overlap), force(force), pMu(pMu), p3(P3), qMu(qMu),
        oProd(oProd), coeff(coeff)
      { }

    };

    template <typename Param> struct MiddleThreeLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int mu_positive = Param::orthogonal_positive;
      static constexpr int sig_positive = Param::sig_positive;

      constexpr MiddleThreeLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.D, parity);

        /*        A________B
         *   mu   |        |
         *       D|        |C
         *
         *    A is the current point (sid)
         *
         */

        for (int d=0; d<4; d++) x[d] += arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;
        int y[4] = {x[0], x[1], x[2], x[3]};

        int mymu = posDir(arg.mu);
        int point_d = updateCoordsIndex(y, arg.E, mymu, (mu_positive ? -1 : 1));
        int ad_link_nbr_idx = mu_positive ? point_d : e_cb;

        int mysig = posDir(arg.sig);
        int point_c = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        for (int d=0; d<4; d++) y[d] = x[d];
        int point_b = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        int bc_link_nbr_idx = mu_positive ? point_c : point_b;
        int ab_link_nbr_idx = sig_positive ? e_cb : point_b;

        // load the link variable connecting a and b
        Link Uab = arg.link(mysig, ab_link_nbr_idx, sig_positive^(1-parity));

        // load the link variable connecting b and c
        Link Ubc = arg.link(mymu, bc_link_nbr_idx, mu_positive^(1-parity));

        Link Oy;
        Oy = arg.oProd(posDir(arg.sig), sig_positive ? point_d : point_c, sig_positive^parity);
        if constexpr (!sig_positive) Oy = conj(Oy);

        Link Ow = !mu_positive ? Ubc*Oy : conj(Ubc)*Oy;

        arg.pMu(0, point_b, 1-parity) = Ow;

        arg.p3(0, e_cb, parity) = sig_positive ? Uab*Ow : conj(Uab)*Ow;

        Link Uad = arg.link(mymu, ad_link_nbr_idx, mu_positive^parity);
        if (!mu_positive)  Uad = conj(Uad);

        if constexpr (sig_positive) Oy = Ow*Uad;
        arg.qMu(0, e_cb, parity) = Uad;

        if constexpr (sig_positive) {
          Link oprod = arg.force(arg.sig, e_cb, parity);
          oprod += arg.coeff*Oy;
          arg.force(arg.sig, e_cb, parity) = oprod;
        }

      }
    };


    /**************************middleFiveLinkKernel*****************************
     *
     *
     * Generally we need
     * READ
     *    3 LINKS:         ab_link,     bc_link,    ad_link
     *    3 COLOR MATRIX:  newOprod_at_A, Pmu_at_C,  Qmu_at_D
     * WRITE
     *    4 COLOR MATRIX:  newOprod_at_A, P3_at_A, Pnumu_at_B, Qnumu_at_A
     *
     * Three call variations:
     *   2. full read/write
     *
     *   In all three above case, if the direction sig is negative, newOprod_at_A is
     *   not read in or written out.
     *
     * Therefore the data traffic, in two-number pair (num_of_link, num_of_color_matrix)
     *   Call 2:  (called 192 time, half positive sig, half negative sig)
     *             if (sig is positive):    (3, 7)
     *             else               :     (3, 5)
     *
     * note: Pmu_at_C could actually be read in from D when it is the fresh outer product
     *       and we call it Pmu_at_C to simply naming. This does not affect our data traffic analysis
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *   call 2:     if (sig is positive)  (4, 1)
     *               else                  (3, 0)
     *
     ****************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct MiddleFiveLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge pNuMu;
      Gauge p5;
      Gauge qNuMu;

      const Gauge pMu;
      const Gauge qMu;
      const real coeff;

      static constexpr int overlap = 1;

      MiddleFiveLinkArg(GaugeField &force, GaugeField &pNuMu, GaugeField &P5, GaugeField &qNuMu,
                 const GaugeField &pMu, const GaugeField &qMu, const GaugeField &link,
                 real coeff)
        : BaseForceArg(link, overlap), force(force), pNuMu(pNuMu), p5(P5), qNuMu(qNuMu),
        pMu(pMu), qMu(qMu), coeff(coeff)
      { }

    };

    template <typename Param> struct MiddleFiveLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int nu_positive = Param::orthogonal_positive;
      static constexpr int sig_positive = Param::sig_positive;

      constexpr MiddleFiveLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.D, parity);

        /*        A________B
         *   nu   |        |
         *       D|        |C
         *
         *    A is the current point (sid)
         *
         */

        for (int d=0; d<4; d++) x[d] += arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;
        int y[4] = {x[0], x[1], x[2], x[3]};

        int mynu = posDir(arg.nu);
        int point_d = updateCoordsIndex(y, arg.E, mynu, (nu_positive ? -1 : 1));
        int ad_link_nbr_idx = nu_positive ? point_d : e_cb;

        int mysig = posDir(arg.sig);
        int point_c = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        for (int d=0; d<4; d++) y[d] = x[d];
        int point_b = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        int bc_link_nbr_idx = nu_positive ? point_c : point_b;
        int ab_link_nbr_idx = sig_positive ? e_cb : point_b;

        // load the link variable connecting a and b
        Link Uab = arg.link(mysig, ab_link_nbr_idx, sig_positive^(1-parity));

        // load the link variable connecting b and c
        Link Ubc = arg.link(mynu, bc_link_nbr_idx, nu_positive^(1-parity));

        Link Oy = arg.pMu(0, point_c, parity);

        Link Ow = !nu_positive ? Ubc*Oy : conj(Ubc)*Oy;

        arg.pNuMu(0, point_b, 1-parity) = Ow;

        arg.p5(0, e_cb, parity) = sig_positive ? Uab*Ow : conj(Uab)*Ow;

        Link Uad = arg.link(mynu, ad_link_nbr_idx, nu_positive^parity);
        if (!nu_positive)  Uad = conj(Uad);

        Oy = arg.qMu(0, point_d, 1-parity);
        Link Ox = Oy*Uad;
        arg.qNuMu(0, e_cb, parity) = Ox;

        if constexpr (sig_positive) {
          Oy = Ow * Ox;
          Link oprod = arg.force(arg.sig, e_cb, parity);
          oprod += arg.coeff*Oy;
          arg.force(arg.sig, e_cb, parity) = oprod;
        }

      }
    };

    /********************************allLinkKernel*********************************************
     *
     * In this function we need
     *   READ
     *     3 LINKS:         ad_link, ab_link, bc_link
     *     5 COLOR MATRIX:  Qprev_at_D, oprod_at_C, newOprod_at_A(sig), newOprod_at_D/newOprod_at_A(mu), shortP_at_D
     *   WRITE:
     *     3 COLOR MATRIX:  newOprod_at_A(sig), newOprod_at_D/newOprod_at_A(mu), shortP_at_D,
     *
     * If sig is negative, then we don't need to read/write the color matrix newOprod_at_A(sig)
     *
     * Therefore the data traffic, in two-number pair (num_of_link, num_of_color_matrix)
     *
     *             if (sig is positive):    (3, 8)
     *             else               :     (3, 6)
     *
     * This function is called 384 times, half positive sig, half negative sig
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *             if(sig is positive)      (6,3)
     *             else                     (4,2)
     *
     ************************************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct AllLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge shortP;

      const Gauge oProd;
      const Gauge p5;
      const real coeff;
      const real accumu_coeff;

      static constexpr int overlap = 1;

      AllLinkArg(GaugeField &force, GaugeField &shortP, const GaugeField &oProd, const GaugeField &P5,
                 const GaugeField &link, real coeff, real accumu_coeff)
        : BaseForceArg(link, overlap), force(force), shortP(shortP),
          oProd(oProd), p5(P5),
          coeff(coeff), accumu_coeff(accumu_coeff)
      { }

    };

    template <typename Param> struct AllLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int rho_positive = Param::orthogonal_positive;
      static constexpr int sig_positive = Param::sig_positive;

      constexpr AllLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.D, parity);
        for (int d=0; d<4; d++) x[d] += arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity^arg.oddness_change;

        auto mycoeff = CoeffSign(sig_positive,parity)*arg.coeff;

        int y[4] = {x[0], x[1], x[2], x[3]};
        int mysig = posDir(arg.sig);
        int point_b = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));
        int ab_link_nbr_idx = (sig_positive) ? e_cb : point_b;

        for (int d=0; d<4; d++) y[d] = x[d];

        /*            sig
         *         A________B
         *      rho |      |
         *        D |      |C
         *
         *   A is the current point (sid)
         *
         */

        int rho = rho_positive ? arg.rho : opp_dir(arg.rho);
        int dir = rho_positive ? -1 : 1;

        int point_d = updateCoordsIndex(y, arg.E, rho, dir);
        int point_c = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        Link Uab = arg.link(posDir(arg.sig), ab_link_nbr_idx, sig_positive^(1-parity));
        Link Uad = arg.link(rho, rho_positive ? point_d : e_cb, rho_positive ? 1-parity : parity);
        Link Ubc = arg.link(rho, rho_positive ? point_c : point_b, rho_positive ? parity : 1-parity);
        Link Ox = arg.p5(0, point_d, 1-parity);
        Link Oy = arg.oProd(0, point_c, parity);
        Link Oz = rho_positive ? conj(Ubc)*Oy : Ubc*Oy;

        if (sig_positive) {
          Link force = arg.force(arg.sig, e_cb, parity);
          force += Sign(parity)*mycoeff*Oz*Ox* (rho_positive ? Uad : conj(Uad));
          arg.force(arg.sig, e_cb, parity) = force;
          Oy = Uab*Oz;
        } else {
          Oy = conj(Uab)*Oz;
        }

        Link force = arg.force(rho, rho_positive ? point_d : e_cb, rho_positive ? 1-parity : parity);
        force += Sign(rho_positive ? 1-parity : parity)*mycoeff* (rho_positive ? Oy*Ox : conj(Ox)*conj(Oy));
        arg.force(rho, rho_positive ? point_d : e_cb, rho_positive ? 1-parity : parity) = force;

        Link shortP = arg.shortP(0, point_d, 1-parity);
        shortP += arg.accumu_coeff * (rho_positive ? Uad : conj(Uad)) *Oy;
        arg.shortP(0, point_d, 1-parity) = shortP;
      }
    };

    /***********************************sideLinkKernel***************************
     *
     * In general we need
     * READ
     *    1  LINK:          ad_link
     *    4  COLOR MATRIX:  shortP_at_D, newOprod, P3_at_A, Qprod_at_D,
     * WRITE
     *    2  COLOR MATRIX:  shortP_at_D, newOprod,
     *
     * Two call variations:
     *   1. full read/write
     *   2. when shortP == NULL && Qprod == NULL:
     *          no need to read ad_link/shortP_at_D or write shortP_at_D
     *          Qprod_at_D does not exit and is not read in
     *
     *
     * Therefore the data traffic, in two-number pair (num_of_links, num_of_color_matrix)
     *   Call 1:   (called 192 times)
     *                           (1, 6)
     *
     *   Call 2:   (called 48 times)
     *                           (0, 3)
     *
     * note: newOprod can be at point D or A, depending on if mu is postive or negative
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *   call 1:       (2, 2)
     *   call 2:       (0, 1)
     *
     *********************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct SideLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge shortP;
      Gauge p3;

      const Gauge qProd;
      const real coeff;
      const real accumu_coeff;

      SideLinkArg(GaugeField &force, GaugeField &shortP, const GaugeField &P3,
                 const GaugeField &qProd, const GaugeField &link, real coeff, real accumu_coeff, int overlap)
        : BaseForceArg(link, overlap), force(force), shortP(shortP), p3(P3), qProd(qProd), coeff(coeff), accumu_coeff(accumu_coeff)
      { }

    };

    template <typename Param> struct SideLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int nu_positive = Param::orthogonal_positive;

      constexpr SideLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb ,arg.D, parity);
        for (int d=0; d<4; d++) x[d] = x[d] + arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;

        /*      compute the side link contribution to the momentum
         *
         *             sig
         *          A________B
         *           |       |   nu
         *         D |       |C
         *
         *      A is the current point (x_cb)
         *
         */

        int mynu = posDir(arg.nu);
        int y[4] = {x[0], x[1], x[2], x[3]};
        int point_d = updateCoordsIndex(y, arg.E, mynu, (nu_positive ? -1 : 1));

        Link Oy = arg.p3(0, e_cb, parity);

        int ad_link_nbr_idx = nu_positive ? point_d : e_cb;

        Link Uad = arg.link(mynu, ad_link_nbr_idx, nu_positive^parity);
        Link Ow = nu_positive ? Uad*Oy : conj(Uad)*Oy;

        Link shortP = arg.shortP(0, point_d, 1-parity);
        shortP += arg.accumu_coeff * Ow;
        arg.shortP(0, point_d, 1-parity) = shortP;

        Link Ox = arg.qProd(0, point_d, 1-parity);
        Ow = nu_positive ? Oy*Ox : conj(Ox)*conj(Oy);

        auto mycoeff = CoeffSign(goes_forward(arg.sig), parity)*CoeffSign(goes_forward(arg.nu),parity)*arg.coeff;

        Link oprod = arg.force(nu_positive ? arg.nu : opp_dir(arg.nu), nu_positive ? point_d : e_cb, nu_positive ? 1-parity : parity);
        oprod += mycoeff * Ow;
        arg.force(nu_positive ? arg.nu : opp_dir(arg.nu), nu_positive ? point_d : e_cb, nu_positive ? 1-parity : parity) = oprod;
      }
    };

    /**************************lepageMiddleLinkKernel*****************************
     *
     *
     * Generally we need
     * READ
     *    3 LINKS:         ab_link,     bc_link,    ad_link
     *    3 COLOR MATRIX:  newOprod_at_A, oprod_at_C,  Qprod_at_D
     * WRITE
     *    2 COLOR MATRIX:  newOprod_at_A, P3_at_A
     *
     * Three call variations:
     *   3. when Pmu/Qmu == NULL,   Pmu_at_B and Qmu_at_A are not written out
     *
     *   In all three above case, if the direction sig is negative, newOprod_at_A is
     *   not read in or written out.
     *
     * Therefore the data traffic, in two-number pair (num_of_link, num_of_color_matrix)
     *   Call 3:  (called 48 times, half positive sig, half negative sig)
     *             if (sig is positive):    (3, 5)
     *             else               :     (3, 2) no need to loadQprod_at_D in this case
     *
     * note: oprod_at_C could actually be read in from D when it is the fresh outer product
     *       and we call it oprod_at_C to simply naming. This does not affect our data traffic analysis
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *   call 3:     if (sig is positive)  (4, 1)
     *   (Lepage)    else                  (2, 0)
     *
     ****************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct LepageMiddleLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge p3;

      const Gauge oProd;
      const Gauge qPrev;
      const real coeff;

      static constexpr int overlap = 2;

      LepageMiddleLinkArg(GaugeField &force, GaugeField &P3, const GaugeField &oProd,
                 const GaugeField &qPrev, const GaugeField &link,
                 real coeff)
        : BaseForceArg(link, overlap), force(force), p3(P3),
        oProd(oProd), qPrev(qPrev), coeff(coeff)
      { }

    };

    template <typename Param> struct LepageMiddleLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int mu_positive = Param::orthogonal_positive;
      static constexpr int sig_positive = Param::sig_positive;

      constexpr LepageMiddleLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.D, parity);

        /*        A________B
         *   mu   |        |
         *       D|        |C
         *
         *    A is the current point (sid)
         *
         */

        for (int d=0; d<4; d++) x[d] += arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;
        int y[4] = {x[0], x[1], x[2], x[3]};

        int mymu = posDir(arg.mu);
        int point_d = updateCoordsIndex(y, arg.E, mymu, (mu_positive ? -1 : 1));
        int ad_link_nbr_idx = mu_positive ? point_d : e_cb;

        int mysig = posDir(arg.sig);
        int point_c = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        for (int d=0; d<4; d++) y[d] = x[d];
        int point_b = updateCoordsIndex(y, arg.E, mysig, (sig_positive ? 1 : -1));

        int bc_link_nbr_idx = mu_positive ? point_c : point_b;
        int ab_link_nbr_idx = sig_positive ? e_cb : point_b;

        // load the link variable connecting a and b
        Link Uab = arg.link(mysig, ab_link_nbr_idx, sig_positive^(1-parity));

        // load the link variable connecting b and c
        Link Ubc = arg.link(mymu, bc_link_nbr_idx, mu_positive^(1-parity));

        Link Oy = arg.oProd(0, point_c, parity);

        Link Ow = !mu_positive ? Ubc*Oy : conj(Ubc)*Oy;

        arg.p3(0, e_cb, parity) = sig_positive ? Uab*Ow : conj(Uab)*Ow;

        Link Uad = arg.link(mymu, ad_link_nbr_idx, mu_positive^parity);
        if constexpr (!mu_positive)  Uad = conj(Uad);

        if constexpr ( sig_positive ) {
          Oy = arg.qPrev(0, point_d, 1-parity);
          Link Ox = Oy*Uad;
          Oy = Ow*Ox;

          Link oprod = arg.force(arg.sig, e_cb, parity);
          oprod += arg.coeff*Oy;
          arg.force(arg.sig, e_cb, parity) = oprod;
        }

      }
    };

    /***********************************lepageSideLinkKernel***************************
     *
     * In general we need
     * READ
     *    1  LINK:          ad_link
     *    4  COLOR MATRIX:  shortP_at_D, newOprod, P3_at_A, Qprod_at_D,
     * WRITE
     *    2  COLOR MATRIX:  shortP_at_D, newOprod,
     *
     * Two call variations:
     *   1. full read/write
     *   2. when shortP == NULL && Qprod == NULL:
     *          no need to read ad_link/shortP_at_D or write shortP_at_D
     *          Qprod_at_D does not exit and is not read in
     *
     *
     * Therefore the data traffic, in two-number pair (num_of_links, num_of_color_matrix)
     *   Call 1:   (called 192 times)
     *                           (1, 6)
     *
     *   Call 2:   (called 48 times)
     *                           (0, 3)
     *
     * note: newOprod can be at point D or A, depending on if mu is postive or negative
     *
     * Flop count, in two-number pair (matrix_multi, matrix_add)
     *   call 1:       (2, 2)
     *   call 2:       (0, 1)
     *
     *********************************************************************************/
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct LepageSideLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge shortP;
      Gauge p3;

      const Gauge qProd;
      const real coeff;
      const real accumu_coeff;

      LepageSideLinkArg(GaugeField &force, GaugeField &shortP, const GaugeField &P3,
                 const GaugeField &qProd, const GaugeField &link, real coeff, real accumu_coeff, int overlap)
        : BaseForceArg(link, overlap), force(force), shortP(shortP), p3(P3), qProd(qProd), coeff(coeff), accumu_coeff(accumu_coeff)
      { }

    };

    template <typename Param> struct LepageSideLink
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int mu_positive = Param::orthogonal_positive;

      constexpr LepageSideLink(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb ,arg.D, parity);
        for (int d=0; d<4; d++) x[d] = x[d] + arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;

        /*      compute the side link contribution to the momentum
         *
         *             sig
         *          A________B
         *           |       |   mu
         *         D |       |C
         *
         *      A is the current point (x_cb)
         *
         */

        int mymu = posDir(arg.mu);
        int y[4] = {x[0], x[1], x[2], x[3]};
        int point_d = updateCoordsIndex(y, arg.E, mymu, (mu_positive ? -1 : 1));

        Link Oy = arg.p3(0, e_cb, parity);

        int ad_link_nbr_idx = mu_positive ? point_d : e_cb;

        Link Uad = arg.link(mymu, ad_link_nbr_idx, mu_positive^parity);
        Link Ow = mu_positive ? Uad*Oy : conj(Uad)*Oy;

        Link shortP = arg.shortP(0, point_d, 1-parity);
        shortP += arg.accumu_coeff * Ow;
        arg.shortP(0, point_d, 1-parity) = shortP;

        Link Ox = arg.qProd(0, point_d, 1-parity);
        Ow = mu_positive ? Oy*Ox : conj(Ox)*conj(Oy);

        auto mycoeff = CoeffSign(goes_forward(arg.sig), parity)*CoeffSign(goes_forward(arg.mu),parity)*arg.coeff;

        Link oprod = arg.force(mu_positive ? arg.mu : opp_dir(arg.mu), mu_positive ? point_d : e_cb, mu_positive ? 1-parity : parity);
        oprod += mycoeff * Ow;
        arg.force(mu_positive ? arg.mu : opp_dir(arg.mu), mu_positive ? point_d : e_cb, mu_positive ? 1-parity : parity) = oprod;
      }
    };


    // Flop count, in two-number pair (matrix_mult, matrix_add)
    // 		(0,1)
    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct SideLinkShortArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge force;
      Gauge p3;

      const real coeff;

      static constexpr int overlap = 1;

      SideLinkShortArg(GaugeField &force, GaugeField &P3, const GaugeField &link,
                 real coeff)
        : BaseForceArg(link, overlap), force(force), p3(P3), coeff(coeff)
      {  }

    };

    template <typename Param> struct SideLinkShort
    {
      using Arg = typename Param::Arg;
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      static constexpr int mu_positive = Param::orthogonal_positive;

      constexpr SideLinkShort(const Param &param) : arg(param.arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.D, parity);
        for (int d=0; d<4; d++) x[d] = x[d] + arg.base_idx[d];
        int e_cb = linkIndex(x,arg.E);
        parity = parity ^ arg.oddness_change;

        /*      compute the side link contribution to the momentum
         *
         *             sig
         *          A________B
         *           |       |   mu
         *         D |       |C
         *
         *      A is the current point (x_cb)
         *
         */
        int mymu = posDir(arg.mu);
        int y[4] = {x[0], x[1], x[2], x[3]};
        int point_d = mu_positive ? updateCoordsIndex(y, arg.E, mymu, (mu_positive ? -1 : 1)) : e_cb;

        int parity_ = mu_positive ? 1-parity : parity;
        auto mycoeff = CoeffSign(goes_forward(arg.sig),parity)*CoeffSign(goes_forward(arg.mu),parity)*arg.coeff;

        Link Oy = arg.p3(0, e_cb, parity);
        Link oprod = arg.force(posDir(arg.mu), point_d, parity_);
        oprod += mu_positive ? mycoeff * Oy : mycoeff * conj(Oy);
        arg.force(posDir(arg.mu), point_d, parity_) = oprod;
      }
    };

    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct CompleteForceArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge outA;        // force output accessor
      const Gauge oProd; // force input accessor
      const real coeff;

      CompleteForceArg(GaugeField &force, const GaugeField &link)
        : BaseForceArg(link, 0), outA(force), oProd(force), coeff(0.0)
      { }

    };

    template <typename Arg> struct CompleteForce
    {
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      constexpr CompleteForce(const Arg &arg) : arg(arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      // Flops count: 4 matrix multiplications per lattice site = 792 Flops per site
      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        getCoords(x, x_cb, arg.X, parity);

        for (int d=0; d<4; d++) x[d] += arg.border[d];
        int e_cb = linkIndex(x,arg.E);

#pragma unroll
        for (int sig=0; sig<4; ++sig) {
          Link Uw = arg.link(sig, e_cb, parity);
          Link Ox = arg.oProd(sig, e_cb, parity);
          Link Ow = Uw*Ox;

          makeAntiHerm(Ow);

          typename Arg::real coeff = (parity==1) ? -1.0 : 1.0;
          arg.outA(sig, e_cb, parity) = coeff*Ow;
        }
      }
    };

    template <typename store_t, int nColor_, QudaReconstructType recon>
    struct LongLinkArg : public BaseForceArg<store_t, nColor_, recon> {
      using BaseForceArg = BaseForceArg<store_t, nColor_, recon>;
      using real = typename mapper<store_t>::type;
      static constexpr int nColor = nColor_;
      using Gauge = typename gauge_mapper<real, recon>::type;

      Gauge outA;
      const Gauge oProd;
      const real coeff;

      LongLinkArg(GaugeField &newOprod, const GaugeField &link, const GaugeField &oprod, real coeff)
        : BaseForceArg(link,0), outA(newOprod), oProd(oprod), coeff(coeff)
      { }

    };

    template <typename Arg> struct LongLink
    {
      using Link = Matrix<complex<typename Arg::real>, Arg::nColor>;
      const Arg &arg;
      constexpr LongLink(const Arg &arg) : arg(arg) {}
      constexpr static const char *filename() { return KERNEL_FILE; }

      // Flops count, in two-number pair (matrix_mult, matrix_add)
      // 				   (24, 12)
      // 4968 Flops per site in total
      __device__ __host__ void operator()(int x_cb, int parity)
      {
        int x[4];
        int dx[4] = {0,0,0,0};

        getCoords(x, x_cb, arg.X, parity);

        for (int i=0; i<4; i++) x[i] += arg.border[i];
        int e_cb = linkIndex(x,arg.E);

        /*
         *
         *    A   B    C    D    E
         *    ---- ---- ---- ----
         *
         *   ---> sig direction
         *
         *   C is the current point (sid)
         *
         */

        // compute the force for forward long links
#pragma unroll
        for (int sig=0; sig<4; sig++) {
          int point_c = e_cb;

          dx[sig]++;
          int point_d = linkIndexShift(x,dx,arg.E);

          dx[sig]++;
          int point_e = linkIndexShift(x,dx,arg.E);

          dx[sig] = -1;
          int point_b = linkIndexShift(x,dx,arg.E);

          dx[sig]--;
          int point_a = linkIndexShift(x,dx,arg.E);
          dx[sig] = 0;

          Link Uab = arg.link(sig, point_a, parity);
          Link Ubc = arg.link(sig, point_b, 1-parity);
          Link Ude = arg.link(sig, point_d, 1-parity);
          Link Uef = arg.link(sig, point_e, parity);

          Link Oz = arg.oProd(sig, point_c, parity);
          Link Oy = arg.oProd(sig, point_b, 1-parity);
          Link Ox = arg.oProd(sig, point_a, parity);

          Link temp = Ude*Uef*Oz - Ude*Oy*Ubc + Ox*Uab*Ubc;

          Link force = arg.outA(sig, e_cb, parity);
          arg.outA(sig, e_cb, parity) = force + arg.coeff*temp;
        } // loop over sig
      }
    };

  }
}
