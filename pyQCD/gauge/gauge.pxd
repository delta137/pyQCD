from pyQCD.core.atomics cimport Real
from pyQCD.core.core cimport lattice_colour_matrix
from pyQCD.core.layout cimport Layout


cdef extern from "types.hpp" namespace "pyQCD::python":
    cdef cppclass _GaugeAction "pyQCD::python::GaugeAction":
        _GaugeAction(const Real, const Layout&) except +

    cdef cppclass _WilsonGaugeAction "pyQCD::python::WilsonGaugeAction"(_GaugeAction):
        _WilsonGaugeAction(const Real, const Layout&) except +

cdef extern from "gauge/plaquette.hpp" namespace "pyQCD::gauge":
    cdef Real _average_plaquette "pyQCD::gauge::average_plaquette"(const lattice_colour_matrix.LatticeColourMatrix&) except +
    
cdef extern from "gauge/rectangle.hpp" namespace "pyQCD::gauge":
    cdef Real _average_rectangle "pyQCD::gauge::average_rectangle"(const lattice_colour_matrix.LatticeColourMatrix&) except +

cdef class GaugeAction:
    cdef _GaugeAction* instance

cdef class WilsonGaugeAction(GaugeAction):
    pass
