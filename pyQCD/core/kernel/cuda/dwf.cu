#include <dwf.h>

DWF::DWF(const float mass, const float M5, const int Ls,
	 const int kernelType, const int L, const int T,
	 const bool precondition, const bool hermitian,
	 const Complex boundaryConditions[4], Complex* links)
  : LinearOperator(L, T, precondition, hermitian, links, true)
{
  this->mass_ = mass;
  this->Ls_ = Ls;
  this->M5_ = M5;
  this->N = 12 * L * L * L * T * Ls;
  this->num_rows = this->N;
  this->num_cols = this->N;

  if (kernelType == 0)
    this->kernel_ = new Wilson(-M5, L, T, false, false, boundaryConditions,
			       this->links_, false);
  else if (kernelType == 1)
    this->kernel_ = new HamberWu(-M5, L, T, false, false, boundaryConditions,
				 this->links_, false);
  else if (kernelType == 2)
    this->kernel_ = new Naik(-M5, L, T, false, false, boundaryConditions,
			     this->links_, false);
  else
    this->kernel_ = new Wilson(-M5, L, T, false, false, boundaryConditions,
			       this->links_, false);
}



DWF::~DWF()
{
  delete this->kernel_;
}



void DWF::apply(Complex* y, const Complex* x) const
{  
  int dimBlock;
  int dimGrid;

  int n = this->T_ * this->L_ * this->L_ * this->L_ * 12;

  Complex* z;
  cudaMalloc((void**) &z, n * sizeof(Complex));

  setGridAndBlockSize(dimBlock, dimGrid, n);

  for (int i = 0; i < this->Ls_; ++i) {
    this->kernel_->apply(y + i * n, x + i * n);
    saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, x + i * n, 1.0, n);
    
    if (i == 0) {
      applyPminus<<<dimGrid,dimBlock>>>(z, x + n, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, -1.0, n);
      applyPplus<<<dimGrid,dimBlock>>>(z, x + n * (this->Ls_ - 1), this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, this->mass_, n);
    }
    else if (i == this->Ls_ - 1) {
      applyPplus<<<dimGrid,dimBlock>>>(z, x + n * (this->Ls_ - 2), this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, -1.0, n);
      applyPminus<<<dimGrid,dimBlock>>>(z, x, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, this->mass_, n);
    }
    else {
      applyPminus<<<dimGrid,dimBlock>>>(z, x + (i + 1) * n, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, -1.0, n);
      applyPplus<<<dimGrid,dimBlock>>>(z, x + (i - 1) * n, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y + i * n, z, -1.0, n);
    }
  }

  cudaFree(z);
}



void DWF::applyHermitian(Complex* y, const Complex* x) const
{
  this->apply(y, x);
  this->makeHermitian(y, y);
}



void DWF::makeHermitian(Complex* y, const Complex* x) const
{
  int dimBlock;
  int dimGrid;

  int n = this->T_ * this->L_ * this->L_ * this->L_ * 12;

  Complex* z;
  cudaMalloc((void**) &z, n * sizeof(Complex));

  // Temporary stores for 4D slices so we can reduce memory usage
  Complex* x0; // First slice
  cudaMalloc((void**) &x0, n * sizeof(Complex));
  Complex* xim1; // slice i - 1
  cudaMalloc((void**) &xim1, n * sizeof(Complex));
  Complex* xi; // ith slice
  cudaMalloc((void**) &xi, n * sizeof(Complex));

  setGridAndBlockSize(dimBlock, dimGrid, n);

  for (int i = 0; i < this->Ls_; ++i) {
    Complex* y_ptr = y + i * n; // The current 4D slices we're working on
    const Complex* x_ptr = x + i * n;
    assignDev<<<dimGrid, dimBlock>>>(xi, x_ptr, n);

    applyGamma5<<<dimGrid, dimBlock>>>(z, xi, this->L_, this->T_);
    this->kernel_->apply(y_ptr, z);
    saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, 1.0, n);
    applyGamma5<<<dimGrid,dimBlock>>>(y_ptr, y_ptr, this->L_, this->T_);    

    if (i == 0) {
      assignDev<<<dimGrid, dimBlock>>>(x0, xi, n);

      applyPplus<<<dimGrid,dimBlock>>>(z, x + n, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, -1.0, n);
      applyPminus<<<dimGrid,dimBlock>>>(z, x + n * (this->Ls_ - 1),
					this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, this->mass_, n);
    }
    else if (i == this->Ls_ - 1) {
      applyPminus<<<dimGrid,dimBlock>>>(z, xim1, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, -1.0, n);
      applyPplus<<<dimGrid,dimBlock>>>(z, x0, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, this->mass_, n);
    }
    else {
      applyPplus<<<dimGrid,dimBlock>>>(z, x_ptr + n, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, -1.0, n);
      applyPminus<<<dimGrid,dimBlock>>>(z, xim1, this->L_, this->T_);
      saxpyDev<<<dimGrid,dimBlock>>>(y_ptr, z, -1.0, n);
    }
    
    assignDev<<<dimGrid, dimBlock>>>(xim1, xi, n);
  }

  cudaFree(z);
  cudaFree(x0);
  cudaFree(xi);
  cudaFree(xim1);
}