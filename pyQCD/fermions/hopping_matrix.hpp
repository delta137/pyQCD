#ifndef PYQCD_HOPPING_MATRIX_HPP
#define PYQCD_HOPPING_MATRIX_HPP
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Created by Matt Spraggs on 02/02/17.
 *
 * Implementation of 4D hopping matrix.
 */

#include <core/qcd_types.hpp>


namespace pyQCD
{
  namespace fermions
  {
    template <typename Real, int Nc, unsigned int Nhops>
    class HoppingMatrix
    {
    public:
      HoppingMatrix(const LatticeColourMatrix <Real, Nc>& gauge_field,
                    const std::vector<std::complex<Real>>& phases);

      HoppingMatrix(const LatticeColourMatrix <Real, Nc>& gauge_field,
                    const std::vector<std::complex<Real>>& phases,
                    const std::vector<Eigen::MatrixXcd>& spin_structures);

      void set_spin_structures(const std::vector<Eigen::MatrixXcd>& matrices)
      {
        spin_structures_ = matrices;
      }

      void set_spin_structures(std::vector<Eigen::MatrixXcd>&& matrices)
      {
        spin_structures_ = matrices;
      }

      unsigned int num_spins() const { return num_spins_; }

      void apply_full(LatticeColourVector<Real, Nc>& out,
                      const LatticeColourVector<Real, Nc>& in) const;

    private:
      LatticeColourMatrix<Real, Nc> scattered_gauge_field_;
      std::vector<Eigen::MatrixXcd> spin_structures_;
      std::vector<std::vector<Int>> neighbour_array_indices_;
      unsigned int num_spins_;
    };


    template <typename Real, int Nc, unsigned int Nhops>
    HoppingMatrix<Real, Nc, Nhops>::HoppingMatrix(
        const LatticeColourMatrix <Real, Nc> &gauge_field,
        const std::vector<std::complex<Real>>& phases)
      : scattered_gauge_field_(gauge_field.layout(), 2 * gauge_field.num_dims()),
        num_spins_(std::pow(2u, gauge_field.num_dims() / 2))
    {
      auto& layout = gauge_field.layout();
      auto volume = gauge_field.volume();
      neighbour_array_indices_ = std::vector<std::vector<Int>>(
          volume, std::vector<Int>(layout.num_dims() * 2));

      // Scatter the supplied gauge field U_\mu (x) so that when we wish to
      // multiply it with the supplied lattice fermion, there won't be frequent
      // cache misses.
      for (unsigned site_index = 0; site_index < volume; ++site_index) {
        auto arr_index = layout.get_array_index(site_index);

        for (unsigned d = 0; d < layout.num_dims(); ++d) {

          auto site_coords = layout.compute_site_coords(site_index);

          auto phase_fwd = (site_coords[d] + Nhops >= layout.shape()[d]) ?
                           phases[d] : std::complex<Real>(1.0);
          auto phase_bck = (site_coords[d] < Nhops) ?
                           phases[d] : std::complex<Real>(1.0);

          scattered_gauge_field_(site_index, 2 * d) =
              ColourMatrix<double, 3>::Identity() * phase_bck;
          scattered_gauge_field_(site_index, 2 * d + 1) =
              ColourMatrix<double, 3>::Identity() * phase_fwd;

          // Move along the lines connecting the current
          for (unsigned h = 0; h < Nhops; ++h) {
            site_coords[d] += h - Nhops;
            layout.sanitize_site_coords(site_coords);
            scattered_gauge_field_(site_index, 2 * d) *=
                gauge_field(site_coords, d);

            site_coords[d] += Nhops;
            layout.sanitize_site_coords(site_coords);
            scattered_gauge_field_(site_index, 2 * d + 1) *=
                gauge_field(site_coords, d);

            site_coords[d] -= h;
          }

          // Compute coordinates/indices of the neighbours of the current site
          site_coords[d] -= Nhops;
          layout.sanitize_site_coords(site_coords);
          auto site_index_minus =
              layout.get_site_index(layout.get_array_index(site_coords));
          site_coords[d] += 2 * Nhops;
          layout.sanitize_site_coords(site_coords);
          auto site_index_plus =
              layout.get_site_index(layout.get_array_index(site_coords));
          site_coords[d] -= Nhops;

          auto arr_index_minus = layout.get_array_index(site_index_minus);
          auto arr_index_plus = layout.get_array_index(site_index_plus);
          neighbour_array_indices_[arr_index][2 * d] = arr_index_minus;
          neighbour_array_indices_[arr_index][2 * d + 1] = arr_index_plus;
        }
      }
    }


    template <typename Real, int Nc, unsigned int Nhops>
    HoppingMatrix<Real, Nc, Nhops>::HoppingMatrix(
        const LatticeColourMatrix <Real, Nc> &gauge_field,
        const std::vector<std::complex<Real>>& phases,
        const std::vector<Eigen::MatrixXcd>& spin_structures)
      : HoppingMatrix(gauge_field, phases)
    {
      set_spin_structures(spin_structures);
    }


    template <typename Real, int Nc, unsigned int Nhops>
    void HoppingMatrix<Real, Nc, Nhops>::apply_full(
        LatticeColourVector<Real, Nc>& fermion_out,
        const LatticeColourVector<Real, Nc>& fermion_in) const
    {
      auto& layout = fermion_in.layout();
      auto ndims = layout.num_dims();
      auto volume = layout.volume();
      LatticeColourVector<Real, Nc> pre_gather_results(
          layout, ndims * num_spins_ * 2);

      for (unsigned arr_index = 0; arr_index < volume; ++arr_index) {
        for (unsigned mu = 0; mu < ndims; ++mu) {
          Int local_index = 2 * (ndims * arr_index + mu);
          for (unsigned alpha = 0; alpha < num_spins_; ++alpha) {
            for (unsigned beta = 0; beta < num_spins_; ++beta) {
              pre_gather_results[num_spins_ * local_index + 2 * alpha] +=
                  spin_structures_[2 * mu].coeff(alpha, beta) *
                  scattered_gauge_field_[local_index] *
                  fermion_in[ndims * arr_index + beta];
              pre_gather_results[num_spins_ * local_index + 2 * alpha + 1] +=
                  spin_structures_[2 * mu + 1].coeff(alpha, beta) *
                  scattered_gauge_field_[local_index + 1].adjoint() *
                  fermion_in[ndims * arr_index + beta];
            }
          }
        }
      }

      for (unsigned arr_index = 0; arr_index < volume; ++arr_index) {
        for (unsigned mu = 0; mu < ndims; ++mu) {
          auto neighbour_index_plus =
              neighbour_array_indices_[arr_index][2 * mu];
          auto neighbour_index_minus =
              neighbour_array_indices_[arr_index][2 * mu + 1];
          for (unsigned alpha = 0; alpha < num_spins_; ++alpha) {
            Int gather_index =
                2 * (num_spins_ * (ndims * arr_index + mu) + alpha);
            fermion_out[num_spins_ * neighbour_index_plus + alpha] +=
                pre_gather_results[gather_index];
            fermion_out[num_spins_ * neighbour_index_minus + alpha] +=
                pre_gather_results[gather_index + 1];
          }
        }
      }
    }
  }
}

#endif //PYQCD_HOPPING_MATRIX_HPP
