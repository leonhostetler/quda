#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cuda_runtime.h>

#include <invert_quda.h>
#include <quda.h>
#include <util_quda.h>
#include <spinor_quda.h>
#include <gauge_quda.h>

FullGauge cudaGaugePrecise; // precise gauge field
FullGauge cudaGaugeSloppy; // sloppy gauge field

void printGaugeParam(QudaGaugeParam *param) {

  printf("Gauge Params:\n");
  printf("X = %d\n", param->X);
  printf("Y = %d\n", param->Y);
  printf("Z = %d\n", param->Z);
  printf("T = %d\n", param->T);
  printf("anisotropy = %e\n", param->anisotropy);
  printf("gauge_order = %d\n", param->gauge_order);
  printf("cpu_prec = %d\n", param->cpu_prec);
  printf("cuda_prec_precise = %d\n", param->cuda_prec_precise);
  printf("reconstruct_precise = %d\n", param->reconstruct_precise);
  printf("cuda_prec_sloppy = %d\n", param->cuda_prec_sloppy);
  printf("reconstruct_sloppy = %d\n", param->reconstruct_sloppy);
  printf("gauge_fix = %d\n", param->gauge_fix);
  printf("t_boundary = %d\n", param->t_boundary);
  printf("packed_size = %d\n", param->packed_size);
  printf("gaugeGiB = %e\n", param->gaugeGiB);
}

void printInvertParam(QudaInvertParam *param) {
  printf("kappa = %e\n", param->kappa);
  printf("mass_normalization = %d\n", param->mass_normalization);
  printf("inv_type = %d\n", param->inv_type);
  printf("tol = %e\n", param->tol);
  printf("iter = %d\n", param->iter);
  printf("maxiter = %d\n", param->maxiter);
  printf("matpc_type = %d\n", param->matpc_type);
  printf("solution_type = %d\n", param->solution_type);
  printf("preserve_source = %d\n", param->preserve_source);
  printf("cpu_prec = %d\n", param->cpu_prec);
  printf("cuda_prec = %d\n", param->cuda_prec);
  printf("dirac_order = %d\n", param->dirac_order);
  printf("spinorGiB = %e\n", param->spinorGiB);
  printf("gflops = %e\n", param->gflops);
  printf("secs = %f\n", param->secs);
}

void initQuda(int dev)
{
  int deviceCount;
  cudaGetDeviceCount(&deviceCount);
  if (deviceCount == 0) {
    fprintf(stderr, "No devices supporting CUDA.\n");
    exit(EXIT_FAILURE);
  }

  for(int i=0; i<deviceCount; i++) {
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, i);
    fprintf(stderr, "found device %d: %s\n", i, deviceProp.name);
  }

  if(dev<0) {
    dev = deviceCount - 1;
    //dev = 0;
  }

  cudaDeviceProp deviceProp;
  cudaGetDeviceProperties(&deviceProp, dev);
  if (deviceProp.major < 1) {
    fprintf(stderr, "Device %d does not support CUDA.\n", dev);
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "Using device %d: %s\n", dev, deviceProp.name);
  cudaSetDevice(dev);

  cudaGaugePrecise.even = NULL;
  cudaGaugePrecise.odd = NULL;

  cudaGaugeSloppy.even = NULL;
  cudaGaugeSloppy.odd = NULL;

  hSpinor1.spinor = NULL;
  hSpinor1.spinorNorm = NULL;

  hSpinor2.spinor = NULL;
  hSpinor2.spinorNorm = NULL;
}

void loadGaugeQuda(void *h_gauge, QudaGaugeParam *param)
{
  gauge_param = param;

  setCudaGaugeParam();
  if (gauge_param->X != L1 || gauge_param->Y != L2 || gauge_param->Z != L3 || gauge_param->T != L4) {
    printf("QUDA error: dimensions do not match: %d=%d, %d=%d, %d=%d, %d=%d\n", 
	   gauge_param->X, L1, gauge_param->Y, L2, gauge_param->Z, L3, gauge_param->T, L4);
    exit(-1);
  }
  gauge_param->packed_size = (gauge_param->reconstruct_precise == QUDA_RECONSTRUCT_8) ? 8 : 12;

  createGaugeField(&cudaGaugePrecise, h_gauge, gauge_param->reconstruct_precise, gauge_param->cuda_prec_precise);
  gauge_param->gaugeGiB = (float)2*cudaGaugePrecise.packedGaugeBytes/ (1 << 30);
  if (gauge_param->cuda_prec_sloppy != gauge_param->cuda_prec_precise) {
    createGaugeField(&cudaGaugeSloppy, h_gauge, gauge_param->reconstruct_sloppy, gauge_param->cuda_prec_sloppy);
    gauge_param->gaugeGiB = (float)2*cudaGaugeSloppy.packedGaugeBytes/ (1 << 30);
  } else {
    cudaGaugeSloppy = cudaGaugePrecise;
  }

}

void endQuda()
{
  freeSpinorBuffer();
  freeGaugeField(&cudaGaugePrecise);
  freeGaugeField(&cudaGaugeSloppy);
}

void dslashQuda(void *h_out, void *h_in, QudaInvertParam *inv_param, int parity, int dagger)
{
  ParitySpinor in = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor out = allocateParitySpinor(inv_param->cuda_prec);

  loadParitySpinor(in, h_in, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);
  printf("\nnorm = %e\n", normCuda((float*)(in.spinor), Nh*24));  
  dslashCuda(out, cudaGaugePrecise, in, parity, dagger);
  retrieveParitySpinor(h_out, out, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(out);
  freeParitySpinor(in);
}

void MatPCQuda(void *h_out, void *h_in, QudaInvertParam *inv_param)
{
  ParitySpinor in = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor out = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor tmp = allocateParitySpinor(inv_param->cuda_prec);
  
  loadParitySpinor(in, h_in, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);
  MatPCCuda(out, cudaGaugePrecise, in, inv_param->kappa, tmp, inv_param->matpc_type);
  retrieveParitySpinor(h_out, out, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(tmp);
  freeParitySpinor(out);
  freeParitySpinor(in);
}

void MatPCDagQuda(void *h_out, void *h_in, QudaInvertParam *inv_param)
{
  ParitySpinor in = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor out = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor tmp = allocateParitySpinor(inv_param->cuda_prec);
  
  loadParitySpinor(in, h_in, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);
  MatPCDagCuda(out, cudaGaugePrecise, in, inv_param->kappa, tmp, inv_param->matpc_type);
  retrieveParitySpinor(h_out, out, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(tmp);
  freeParitySpinor(out);
  freeParitySpinor(in);
}

void MatPCDagMatPCQuda(void *h_out, void *h_in, QudaInvertParam *inv_param)
{
  ParitySpinor in = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor out = allocateParitySpinor(inv_param->cuda_prec);
  ParitySpinor tmp = allocateParitySpinor(inv_param->cuda_prec);
  
  loadParitySpinor(in, h_in, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);  
  MatPCDagMatPCCuda(out, cudaGaugePrecise, in, inv_param->kappa, tmp, inv_param->matpc_type);
  retrieveParitySpinor(h_out, out, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(tmp);
  freeParitySpinor(out);
  freeParitySpinor(in);
}

void MatQuda(void *h_out, void *h_in, QudaInvertParam *inv_param) {
  FullSpinor in, out;

  in.even = allocateParitySpinor(inv_param->cuda_prec);
  in.odd = allocateParitySpinor(inv_param->cuda_prec);
  out.even = allocateParitySpinor(inv_param->cuda_prec);
  out.odd = allocateParitySpinor(inv_param->cuda_prec);

  loadSpinorField(in, h_in, inv_param->cpu_prec, inv_param->cuda_prec, inv_param->dirac_order);

  dslashCuda(out.odd, cudaGaugePrecise, in.even, 1, 0);
  dslashCuda(out.even, cudaGaugePrecise, in.odd, 0, 0);

  xpayCuda((float*)in.even.spinor, -inv_param->kappa, (float*)out.even.spinor, Nh*spinorSiteSize);
  xpayCuda((float*)in.odd.spinor, -inv_param->kappa, (float*)out.odd.spinor, Nh*spinorSiteSize);

  retrieveSpinorField(h_out, out, inv_param->cpu_prec, 
		      inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(in.even);
  freeParitySpinor(in.odd);
  freeParitySpinor(out.even);
  freeParitySpinor(out.odd);
}

void MatDagQuda(void *h_out, void *h_in, QudaInvertParam *inv_param) {
  FullSpinor in, out;

  in.even = allocateParitySpinor(inv_param->cuda_prec);
  in.odd = allocateParitySpinor(inv_param->cuda_prec);
  out.even = allocateParitySpinor(inv_param->cuda_prec);
  out.odd = allocateParitySpinor(inv_param->cuda_prec);

  loadSpinorField(in, h_in, inv_param->cpu_prec, 
		  inv_param->cuda_prec, inv_param->dirac_order);

  dslashCuda(out.odd, cudaGaugePrecise, in.even, 1, 1);
  dslashCuda(out.even, cudaGaugePrecise, in.odd, 0, 1);

  xpayCuda((float*)in.even.spinor, -inv_param->kappa, (float*)out.even.spinor, Nh*spinorSiteSize);
  xpayCuda((float*)in.odd.spinor, -inv_param->kappa, (float*)out.odd.spinor, Nh*spinorSiteSize);

  retrieveSpinorField(h_out, out, inv_param->cpu_prec, 
		      inv_param->cuda_prec, inv_param->dirac_order);

  freeParitySpinor(in.even);
  freeParitySpinor(in.odd);
  freeParitySpinor(out.even);
  freeParitySpinor(out.odd);
}

void invertQuda(void *h_x, void *h_b, QudaInvertParam *param)
{
  invert_param = param;

  if (param->cuda_prec == QUDA_DOUBLE_PRECISION) {
    printf("Sorry, only double precision not yet supported\n");
    exit(-1);
  }

  if (param->cpu_prec == QUDA_HALF_PRECISION) {
    printf("Half precision not supported on cpu\n");
    exit(-1);
  }

  int slenh = Nh*spinorSiteSize;

  float spinorGiB = (float)slenh*sizeof(float) / (1 << 30);
  if (param->preserve_source == QUDA_PRESERVE_SOURCE_NO)
    spinorGiB *= (param->inv_type == QUDA_CG_INVERTER ? 5 : 7);
  else
    spinorGiB *= (param->inv_type == QUDA_CG_INVERTER ? 8 : 9);
  param->spinorGiB = spinorGiB;

  param->secs = 0;
  param->gflops = 0;
  param->iter = 0;

  float kappa = param->kappa;

  FullSpinor b, x;
  ParitySpinor in = allocateParitySpinor(invert_param->cuda_prec); // source vector
  ParitySpinor out = allocateParitySpinor(invert_param->cuda_prec); // solution vector
  ParitySpinor tmp = allocateParitySpinor(invert_param->cuda_prec); // temporary used when applying operator

  if (param->solution_type == QUDA_MAT_SOLUTION) {
    if (param->preserve_source == QUDA_PRESERVE_SOURCE_YES) {
      b.even = allocateParitySpinor(invert_param->cuda_prec);
      b.odd = allocateParitySpinor(invert_param->cuda_prec);
    } else {
      b.even = out;
      b.odd = tmp;
    }

    if (param->matpc_type == QUDA_MATPC_EVEN_EVEN) { x.odd = tmp; x.even = out; }
    else { x.even = tmp; x.odd = out; }

    loadSpinorField(b, h_b, param->cpu_prec, param->cuda_prec, param->dirac_order);

    // multiply the source to get the mass normalization
    if (param->mass_normalization == QUDA_MASS_NORMALIZATION) {
      axCuda(2*kappa, (float *)b.even.spinor, slenh);
      axCuda(2*kappa, (float *)b.odd.spinor, slenh);
    }

    if (param->matpc_type == QUDA_MATPC_EVEN_EVEN) {
      dslashXpaySCuda(in, cudaGaugePrecise, b.odd, 0, 0, b.even, kappa);
    } else {
      dslashXpaySCuda(in, cudaGaugePrecise, b.even, 1, 0, b.odd, kappa);
    }

  } else if (param->solution_type == QUDA_MATPC_SOLUTION || 
	     param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION){
    loadParitySpinor(in, h_b, param->cpu_prec, param->cuda_prec, param->dirac_order);

    // multiply the source to get the mass normalization
    if (param->mass_normalization == QUDA_MASS_NORMALIZATION)
      if (param->solution_type == QUDA_MATPC_SOLUTION) 
	axCuda(4*kappa*kappa, (float *)in.spinor, slenh);
      else
	axCuda(16*pow(kappa,4), (float *)in.spinor, slenh);
  }

  switch (param->inv_type) {
  case QUDA_CG_INVERTER:
    if (param->solution_type != QUDA_MATPCDAG_MATPC_SOLUTION) {
      copyCuda((float *)out.spinor, (float *)in.spinor, slenh);
      MatPCDagCuda(in, cudaGaugePrecise, out, kappa, tmp, param->matpc_type);
    }
    invertCgCuda(out, in, cudaGaugeSloppy, tmp, param);
    break;
  case QUDA_BICGSTAB_INVERTER:
    if (param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION) {
      invertBiCGstabCuda(out, in, cudaGaugeSloppy, cudaGaugePrecise, tmp, param, QUDA_DAG_YES);
      copyCuda((float *)in.spinor, (float *)out.spinor, slenh);
    }
    invertBiCGstabCuda(out, in, cudaGaugeSloppy, cudaGaugePrecise, tmp, param, QUDA_DAG_NO);
    break;
  default:
    printf("Inverter type %d not implemented\n", param->inv_type);
    exit(-1);
  }

  if (param->solution_type == QUDA_MAT_SOLUTION) {

    if (param->preserve_source == QUDA_PRESERVE_SOURCE_NO) {
      // qdp dirac fields are even-odd ordered
      b.even = in;
      loadSpinorField(b, h_b, param->cpu_prec, param->cuda_prec, param->dirac_order);
    }

    if (param->matpc_type == QUDA_MATPC_EVEN_EVEN) {
      dslashXpaySCuda(x.odd, cudaGaugePrecise, out, 1, 0, b.odd, kappa);
    } else {
      dslashXpaySCuda(x.even, cudaGaugePrecise, out, 0, 0, b.even, kappa);
    }

    retrieveSpinorField(h_x, x, param->cpu_prec, param->cuda_prec, param->dirac_order);

    if (param->preserve_source == QUDA_PRESERVE_SOURCE_YES) freeSpinorField(b);

  } else {
    retrieveParitySpinor(h_x, out, param->cpu_prec, param->cuda_prec, param->dirac_order);
  }

  freeParitySpinor(tmp);
  freeParitySpinor(in);
  freeParitySpinor(out);

  return;
}

