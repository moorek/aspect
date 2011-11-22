/* $Id: step-32.cc 234 2011-10-19 18:07:35Z bangerth $ */
/* Author: Martin Kronbichler, Uppsala University,
           Wolfgang Bangerth, Texas A&M University,
     Timo Heister, University of Goettingen, 2008-2011 */
/*                                                                */
/*    Copyright (C) 2008, 2009, 2010, 2011 by the deal.II authors */
/*                                                                */
/*    This file is subject to QPL and may not be  distributed     */
/*    without copyright and license information. Please refer     */
/*    to the file deal.II/doc/license.html for the  text  and     */
/*    further information on this license.                        */

#include <aspect/simulator.h>
#include <aspect/global.h>
#include <aspect/equation_data.h>

#include <deal.II/base/parameter_handler.h>



namespace aspect
{
  template <int dim>
  Simulator<dim>::Parameters::Parameters (ParameterHandler &prm)
  {
    parse_parameters (prm);
  }


  template <int dim>
  void
  Simulator<dim>::Parameters::
  declare_parameters (ParameterHandler &prm)
  {
    prm.declare_entry ("Resume computation", "false",
                       Patterns::Bool (),
                       "Whether the last computation be resumed from a saved checkpoint or not.");

    prm.declare_entry ("End time", "1e8",
                       Patterns::Double (0),
                       "The end time of the simulation in years.");

    prm.declare_entry ("Time step scaling", "1.0",
                       Patterns::Double (0),
                       "Time step size is given as scaling/cfl_number, default: 1.0");

    prm.enter_subsection ("Mesh refinement");
    {
      prm.declare_entry ("Initial global refinement", "2",
                         Patterns::Integer (0),
                         "The number of global refinement steps performed on "
                         "the initial coarse mesh, before the problem is first "
                         "solved there.");
      prm.declare_entry ("Initial adaptive refinement", "2",
                         Patterns::Integer (0),
                         "The number of adaptive refinement steps performed after "
                         "initial global refinement.");
      prm.declare_entry ("Time steps between mesh refinement", "10",
                         Patterns::Integer (1),
                         "The number of time steps after which the mesh is to be "
                         "adapted based on computed error indicators.");
      prm.declare_entry ("Refinement fraction", "0.3",
                         Patterns::Double(0,1),
                         "The fraction of cells with the largest error that "
                         "should be flagged for refinement.");
      prm.declare_entry ("Coarsening fraction", "0.05",
                         Patterns::Double(0,1),
                         "The fraction of cells with the smallest error that "
                         "should be flagged for coarsening.");
      prm.declare_entry ("Additional refinement times", "",
                         Patterns::List (Patterns::Double(0)),
                         "A list of times (in years) so that if the end time of a time step "
                         "is beyond this time, an additional round of mesh refinement "
                         "is triggered. This is mostly useful to make sure we "
                         "can get through the initial transient phase of a simulation "
                         "on a relatively coarse mesh, and then refine again when we "
                         "are in a time range that we are interested in and where "
                         "we would like to use a finer mesh.");
    }
    prm.leave_subsection();

    prm.enter_subsection ("Stabilization parameters");
    {
      prm.declare_entry ("alpha", "2",
                         Patterns::Double (1, 2),
                         "The exponent in the entropy viscosity stabilization.");
      prm.declare_entry ("c_R", "0.11",
                         Patterns::Double (0),
                         "The c_R factor in the entropy viscosity "
                         "stabilization.");
      prm.declare_entry ("beta", "0.078",
                         Patterns::Double (0),
                         "The beta factor in the artificial viscosity "
                         "stabilization. An appropriate value for 2d is 0.052 "
                         "and 0.078 for 3d.");
    }
    prm.leave_subsection ();

    prm.enter_subsection ("ModelSettings");
    {
      prm.declare_entry ("Include shear heating", "true",
                         Patterns::Bool (),
                         "Whether to include shear heating into the model or not. From a "
                         "physical viewpoint, shear heating should always be used but may "
                         "be undesirable when comparing results with known benchmarks that "
                         "do not include this term in the temperature equation.");
    }
    prm.leave_subsection ();

    prm.enter_subsection ("ModelParameters");
    {
      prm.declare_entry ("kappa", "4.548e-7",
                         Patterns::Double (),
                         "thermal diffusivity (k/(rho*cp)");
      prm.declare_entry ("Radiogenic heating rate", "0e0",
                         Patterns::Double (),
                         "H0");
      prm.declare_entry ("T1", "0",
                         Patterns::Double (),
                         "temperature at outer boundary (lythosphere water/air)");
      prm.declare_entry ("T0", "6000",
                         Patterns::Double (),
                         "temperature at inner boundary (core mantle boundary");
    }
    prm.leave_subsection ();


    prm.enter_subsection ("Discretization");
    {
      prm.declare_entry ("Stokes velocity polynomial degree", "2",
                         Patterns::Integer (1),
                         "The polynomial degree to use for the velocity variables "
                         "in the Stokes system.");
      prm.declare_entry ("Temperature polynomial degree", "2",
                         Patterns::Integer (1),
                         "The polynomial degree to use for the temperature variable.");
      prm.declare_entry ("Use locally conservative discretization", "true",
                         Patterns::Bool (),
                         "Whether to use a Stokes discretization that is locally "
                         "conservative at the expense of a larger number of degrees "
                         "of freedom, or to go with a cheaper discretization "
                         "that does not locally conserve mass (although it is "
                         "globally conservative.");
    }
    prm.leave_subsection ();
  }



  template <int dim>
  void
  Simulator<dim>::Parameters::
  parse_parameters (ParameterHandler &prm)
  {
    resume_computation      = prm.get_bool ("Resume computation");
    end_time                = prm.get_double ("End time");
    time_step_scaling       = prm.get_double ("Time step scaling");

    prm.enter_subsection ("Mesh refinement");
    {
      initial_global_refinement   = prm.get_integer ("Initial global refinement");
      initial_adaptive_refinement = prm.get_integer ("Initial adaptive refinement");

      adaptive_refinement_interval= prm.get_integer ("Time steps between mesh refinement");
      refinement_fraction         = prm.get_double ("Refinement fraction");
      coarsening_fraction         = prm.get_double ("Coarsening fraction");

      // extract the list of times at which additional refinement is requested
      // then sort it and convert it to seconds
      additional_refinement_times
        = Utilities::string_to_double
          (Utilities::split_string_list(prm.get ("Additional refinement times")));
      std::sort (additional_refinement_times.begin(),
                 additional_refinement_times.end());
      for (unsigned int i=0; i<additional_refinement_times.size(); ++i)
        additional_refinement_times[i] *= year_in_seconds;
    }
    prm.leave_subsection ();

    prm.enter_subsection ("Stabilization parameters");
    {
      stabilization_alpha = prm.get_double ("alpha");
      stabilization_c_R   = prm.get_double ("c_R");
      stabilization_beta  = prm.get_double ("beta");
    }
    prm.leave_subsection ();

    prm.enter_subsection ("ModelSettings");
    {
      include_shear_heating = prm.get_bool ("Include shear heating");
    }
    prm.leave_subsection ();

    prm.enter_subsection ("ModelParameters");
    {
      EquationData::kappa = prm.get_double ("kappa");
      radiogenic_heating_rate = prm.get_double ("Radiogenic heating rate");
      EquationData::T0 = prm.get_double ("T0");
      EquationData::T1 = prm.get_double ("T1");
    }

    prm.leave_subsection ();

    prm.enter_subsection ("Discretization");
    {
      stokes_velocity_degree = prm.get_integer ("Stokes velocity polynomial degree");
      temperature_degree     = prm.get_integer ("Temperature polynomial degree");
      use_locally_conservative_discretization
        = prm.get_bool ("Use locally conservative discretization");
    }
    prm.leave_subsection ();
  }
}


// explicit instantiation of the functions we implement in this file
namespace aspect
{
  template
  class Simulator<deal_II_dimension>::Parameters;
}