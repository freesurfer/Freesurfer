#pragma once

#include <stdexcept>

#include "cudaimage.hpp"

namespace kvl {
  namespace cuda {
    template<typename T>
    void RunVisitCounterSimpleCUDA( CudaImage<int,3,unsigned short>& d_output,
				    const CudaImage<T,3,size_t>& d_tetrahedra ) {
      throw std::runtime_error("Must call RunVisitCounterSimpleCUDA with float or double");
    }

    template<>
    void RunVisitCounterSimpleCUDA( CudaImage<int,3,unsigned short>& d_output,
				    const CudaImage<float,3,size_t>& d_tetrahedra );
  }
}
