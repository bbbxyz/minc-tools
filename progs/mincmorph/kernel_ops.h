/* kernel_ops.h */

#ifndef KERNEL_OPS
#define KERNEL_OPS

#include <volume_io.h>
#include "kernel_io.h"

#define COMPARE(a, b) (((a) > (b)) - ((a) < (b)))

/* kernel functions */
VIO_Volume  binarise(VIO_Volume vol, double floor, double ceil, double fg, double bg);
VIO_Volume  clamp(VIO_Volume vol, double floor, double ceil, double bg);
VIO_Volume  pad(Kernel * K, VIO_Volume vol, double bg);
VIO_Volume  erosion_kernel(Kernel * K, VIO_Volume vol);
VIO_Volume  dilation_kernel(Kernel * K, VIO_Volume vol);
VIO_Volume  median_dilation_kernel(Kernel * K, VIO_Volume vol);
VIO_Volume  m_filter_kernel(Kernel * K, VIO_Volume vol, int method);
VIO_Volume  zero_dilation_kernel(Kernel * K, VIO_Volume vol);
VIO_Volume  convolve_kernel(Kernel * K, VIO_Volume vol);
VIO_Volume  distance_kernel(Kernel * K, VIO_Volume vol, double bg);
VIO_Volume  group_kernel(Kernel * K, VIO_Volume vol, double bg);
VIO_Volume  lcorr_kernel(Kernel * K, VIO_Volume vol, VIO_Volume cmp);

#endif
