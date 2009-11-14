#ifndef _GAUGE_QUDA_H
#define _GAUGE_QUDA_H

#include <enum_quda.h>
#include <dslash_quda.h>

#ifdef __cplusplus
extern "C" {
#endif

  void createGaugeField(FullGauge *cudaGauge, void *cpuGauge, Precision precision, 
			ReconstructType reconstruct, Tboundary t_boundary,
			int *X, double anisotropy, int pad);

  void restoreGaugeField(void *cpuGauge, FullGauge *cudaGauge);

  void freeGaugeField(FullGauge *cudaCauge);
  
#ifdef __cplusplus
}
#endif

#endif // _GAUGE_QUDA_H