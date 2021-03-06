#ifndef PYQCD_HEATBATH_HPP
#define PYQCD_HEATBATH_HPP

/*
 * This file is part of pyQCD.
 *
 * pyQCD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pyQCD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *
 * Created by Matt Spraggs on 10/02/16.
 *
 * This file contains the functions necessary to update a single gauge link
 * using the pseudo heatbath algorithm.
 */

#include <random>

#include <core/qcd_types.hpp>
#include <gauge/gauge_action.hpp>
#include <utils/matrices.hpp>
#include <utils/random.hpp>


namespace pyQCD {

  template <typename Real>
  SU2Matrix<Real> gen_heatbath_su2(const Real weight)
  {
    // Generate a random SU(2) matrix distributed according to the distribution
    // exp(0.5 * weight * beta * Re tr(X)). We use the algorithm specified in
    // Kennedy and Pendleton (1985), in Phys. Lett. 156B.
    //
    // (See also page 87 of Gattringer and Lang for the source material for
    // this algorithm.)

    // Coefficients for the SU(2) basis of Pauli matrices. This final vector
    // will need to be normalised.
    Real coeffs[4];
    // Now we need to fill the components of this vector. The first component
    // must be distributed according to
    //   sqrt(1 - x^2) * exp(weight * beta * x)
    Real lambda_squared = 2.0;
    Real uniform_squared = std::pow(rng().generate_real<Real>(0.0, 1.0), 2);
    while (uniform_squared > 1 - lambda_squared) {
      Real r0 = 1.0 - rng().generate_real<Real>(0.0, 1.0);
      Real r1 = 1.0 - rng().generate_real<Real>(0.0, 1.0);
      Real r2 = 1.0 - rng().generate_real<Real>(0.0, 1.0);
      lambda_squared
        = - 1.0 / (2.0 * weight)
          * (std::log(r0) + std::pow(std::cos(2 * pi * r1), 2) * std::log(r2));
      uniform_squared = std::pow(rng().generate_real<Real>(0.0, 1.0), 2);
    }
    coeffs[0] = 1 - 2 * lambda_squared;
    // With the first component determined, the magnitude of the remaining
    // three-vector can easily be determined.
    Real three_vec_magnitude = std::sqrt(1 - coeffs[0] * coeffs[0]);
    // The remaining three-vector should then be take from a uniform spherical
    // distribution.
    Real cos_theta = rng().generate_real<Real>(-1.0, 1.0);
    Real sin_theta = std::sqrt(1 - cos_theta * cos_theta);
    Real phi = rng().generate_real<Real>(0, 2 * pi);

    coeffs[1] = three_vec_magnitude * sin_theta * std::cos(phi);
    coeffs[2] = three_vec_magnitude * sin_theta * std::sin(phi);
    coeffs[3] = three_vec_magnitude * cos_theta;

    return construct_su2<Real>(coeffs);
  }

  template <typename Real, int Nc>
  void su2_heatbath_update(ColourMatrix<Real, Nc>& link,
                           const ColourMatrix<Real, Nc>& staple,
                           const Real weight, const unsigned int subgroup)
  {
    // Perform an SU(2) heatbath update on the given lattice link
    ColourMatrix<Real, Nc> W = link * staple;
    auto A = extract_su2(W, subgroup);
    auto sqrt_detA = std::sqrt(A.determinant());
    A /= sqrt_detA;
    Real a = sqrt_detA.real();
    bool det_is_zero = a < 6.0 * std::numeric_limits<Real>::epsilon();
    auto X = det_is_zero ? random_su2<Real>() : gen_heatbath_su2(a * weight);
    auto N = insert_su2<Nc>((X * A.adjoint()).eval(), subgroup);
    link = N * link;
  }

  template <typename Real, int Nc>
  void heatbath_link_update(LatticeColourMatrix<Real, Nc> &gauge_field,
                            const gauge::Action<Real, Nc> &action,
                            const Int link_index)
  {
    // Perform SU(N) heatbath update on the specified lattice link
    auto staple = action.compute_staples(gauge_field, link_index);
    auto& link = gauge_field(link_index / gauge_field.site_size(),
                             link_index % gauge_field.site_size());
    Real beta_prime = action.beta() / Nc;

    constexpr int num_subgroups = (Nc * (Nc - 1)) / 2;

    // Here we do the pseudo-heatbath over the subgroups of SU(N)
    for (unsigned int subgroup = 0; subgroup < num_subgroups; ++subgroup) {
      su2_heatbath_update(link, staple, beta_prime, subgroup);
    }
  }


  template <typename Real, int Nc>
  void heatbath_update(LatticeColourMatrix<Real, Nc>& gauge_field,
                       const gauge::Action<Real, Nc>& action,
                       const unsigned int num_iter)
  {
    auto num_links = gauge_field.size();
    for (unsigned int i = 0; i < num_iter; ++i) {
      for (unsigned int link = 0; link < num_links; ++link) {
        heatbath_link_update(gauge_field, action, link);
      }
    }
  }
}

#endif