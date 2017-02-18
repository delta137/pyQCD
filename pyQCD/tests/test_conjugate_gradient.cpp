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
 * Created by Matt Spraggs on 12/02/17.
 *
 * Test of the conjugate gradient algorithm.
 */

#define CATCH_CONFIG_MAIN

#include <algorithms/conjugate_gradient.hpp>
#include <fermions/wilson_action.hpp>

#include "helpers.hpp"


template <typename Real, int Nc>
class TestAction : public pyQCD::fermions::Action<Real, Nc>
{
public:
  TestAction(const Real mass) : pyQCD::fermions::Action<Real, Nc>(mass) {}

  void apply_full(pyQCD::LatticeColourVector<Real, Nc>& fermion_out,
                  const pyQCD::LatticeColourVector<Real, Nc>& fermion_in) const
  { fermion_out = this->mass_ * fermion_in; }

  void apply_hermiticity(pyQCD::LatticeColourVector<Real, Nc>& fermion) const
  { }
  void remove_hermiticity(pyQCD::LatticeColourVector<Real, Nc>& fermion) const
  { }
};


TEST_CASE ("Test of conjugate gradient algorithm")
{
  typedef pyQCD::ColourVector<double, 3> SiteFermion;
  typedef pyQCD::LatticeColourVector<double, 3> LatticeFermion;

  pyQCD::LexicoLayout layout({8, 4, 4, 4});

  LatticeFermion src(layout, SiteFermion::Zero(), 4);
  src[0][0] = 1.0;

  SECTION ("Testing simple proportional action")
  {
    TestAction<double, 3> action(2.0);

    auto result = pyQCD::conjugate_gradient(action, src, 1000, 1e-10);

    for (int i = 0; i < 3; ++i) {
      REQUIRE (std::get<0>(result)[0][i].real() == (i == 0 ? 0.5 : 0.0));
      REQUIRE (std::get<0>(result)[0][i].imag() == 0.0);
    }

    REQUIRE (std::get<1>(result) == 0);
    REQUIRE (std::get<2>(result) == 1);
  }

  SECTION ("Testing Wilson action")
  {
    typedef pyQCD::ColourMatrix<double, 3> GaugeLink;
    typedef pyQCD::LatticeColourMatrix<double, 3> GaugeField;

    GaugeField gauge_field(layout, GaugeLink::Identity(), 4);

    pyQCD::fermions::WilsonAction<double, 3> action(0.1, gauge_field);

    auto result = pyQCD::conjugate_gradient(action, src, 1000, 1e-8);

    MatrixCompare<SiteFermion> compare(1e-8, 1e-12);
    SiteFermion expected = SiteFermion::Zero();
    expected[0] = std::complex<double>(0.2522536470229704,
                                       1.1333971980249629e-13);

    REQUIRE (compare(std::get<0>(result)[0], expected));
    REQUIRE ((std::get<1>(result) < 1e-8 && std::get<1>(result) > 0));
    REQUIRE (std::get<2>(result) == 69);
  }
}