//
// Auto-tuned blas CUDA parameters, generated by blas_test
//

static int blas_threads[23][3] = {
  {  64,   96,   64},  // Kernel  0: copyCuda (high source precision)
  {  96,  992,   64},  // Kernel  1: copyCuda (low source precision)
  { 128,  128,  128},  // Kernel  2: axpbyCuda
  { 128,  128,  128},  // Kernel  3: xpyCuda
  { 128,  128,  128},  // Kernel  4: axpyCuda
  { 128,  128,  128},  // Kernel  5: xpayCuda
  { 128,  128,  128},  // Kernel  6: mxpyCuda
  { 192,  224,  160},  // Kernel  7: axCuda
  { 128,  128,  128},  // Kernel  8: caxpyCuda
  { 128,  160,   96},  // Kernel  9: caxpbyCuda
  { 128,   96,   96},  // Kernel 10: cxpaypbzCuda
  {  96,   64,   64},  // Kernel 11: axpyBzpcxCuda
  {  64,   64,   64},  // Kernel 12: axpyZpbxCuda
  {  64,   96,   64},  // Kernel 13: caxpbypzYmbwCuda
  { 256,  256,  256},  // Kernel 14: normCuda
  { 128,  128,  128},  // Kernel 15: reDotProductCuda
  { 128,  256,  128},  // Kernel 16: axpyNormCuda
  { 128,  256,  128},  // Kernel 17: xmyNormCuda
  { 128,  128,  128},  // Kernel 18: cDotProductCuda
  { 128,  128,  128},  // Kernel 19: xpaycDotzyCuda
  { 128,  128,  128},  // Kernel 20: cDotProductNormACuda
  { 128,  128,  128},  // Kernel 21: cDotProductNormBCuda
  { 128,  128,  256}   // Kernel 22: caxpbypzYmbwcDotProductWYNormYCuda
};

static int blas_blocks[23][3] = {
  { 2048,  2048, 16384},  // Kernel  0: copyCuda (high source precision)
  {32768, 65536,  8192},  // Kernel  1: copyCuda (low source precision)
  { 1024,  4096,  8192},  // Kernel  2: axpbyCuda
  { 1024,  4096, 32768},  // Kernel  3: xpyCuda
  { 1024,  4096, 32768},  // Kernel  4: axpyCuda
  { 1024,  4096, 32768},  // Kernel  5: xpayCuda
  { 1024,  4096, 16384},  // Kernel  6: mxpyCuda
  { 1024,  2048, 16384},  // Kernel  7: axCuda
  { 1024,  4096, 32768},  // Kernel  8: caxpyCuda
  { 1024,  4096, 65536},  // Kernel  9: caxpbyCuda
  { 1024, 65536, 16384},  // Kernel 10: cxpaypbzCuda
  { 2048,  8192,  8192},  // Kernel 11: axpyBzpcxCuda
  { 2048,  8192,  8192},  // Kernel 12: axpyZpbxCuda
  { 2048, 32768, 16384},  // Kernel 13: caxpbypzYmbwCuda
  {   64,    64,    64},  // Kernel 14: normCuda
  {  256,    64,    64},  // Kernel 15: reDotProductCuda
  {  512,    64,    64},  // Kernel 16: axpyNormCuda
  {   64,    64,    64},  // Kernel 17: xmyNormCuda
  {  256,    64,    64},  // Kernel 18: cDotProductCuda
  {   64,    64,    64},  // Kernel 19: xpaycDotzyCuda
  {   64,    64,    64},  // Kernel 20: cDotProductNormACuda
  {   64,    64,    64},  // Kernel 21: cDotProductNormBCuda
  {  128,    64,   256}   // Kernel 22: caxpbypzYmbwcDotProductWYNormYCuda
};
