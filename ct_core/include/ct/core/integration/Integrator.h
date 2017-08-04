/***********************************************************************************
Copyright (c) 2017, Michael Neunert, Markus Giftthaler, Markus Stäuble, Diego Pardo,
Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of ETH ZURICH nor the names of its contributors may be used
      to endorse or promote products derived from this software without specific
      prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL ETH ZURICH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************/


#ifndef CT_CORE_INTEGRATOR_H_
#define CT_CORE_INTEGRATOR_H_

#include <type_traits>
#include <functional>
#include <cmath>

#include "EventHandler.h"
#include "Observer.h"
#include "eigenIntegration.h"

#include "internal/StepperBase.h"

#include "internal/SteppersODEInt.h"
#include "internal/SteppersCT.h"

#include <ct/core/types/AutoDiff.h>

namespace ct {
namespace core {


/**
 * @brief      The available integration types
 */
enum IntegrationType
{
    EULER,
    RK4,
    MODIFIED_MIDPOINT,
    ODE45,
    RK5VARIABLE,
    RK78,
    BULIRSCHSTOER,
    EULERCT,
    RK4CT
};


//! Standard Integrator
/*!
 * A standard Integrator for numerically solving Ordinary Differential Equations (ODEs) of the form
 *
 * \f[
 * \dot{x} = f(x,t)
 * \f]
 *
 *
 * Unit test \ref IntegrationTest.cpp illustrates the use of Integrator.h
 *
 *
 * @tparam STATE_DIM the size of the state vector
 * @tparam SCALAR The scalar type
 */
template <size_t STATE_DIM, typename SCALAR = double>
class Integrator
{
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	typedef std::shared_ptr<EventHandler<STATE_DIM, SCALAR>> EventHandlerPtr;
	typedef std::vector<EventHandlerPtr, Eigen::aligned_allocator<EventHandlerPtr>> EventHandlerPtrVector;	

	//! constructor
	/*!
	 * Creates a standard integrator
	 *
	 * @param system the system (ODE)
	 * @param eventHandlers optional event handler
	 * @param absErrTol optional absolute error tolerance (for variable step solvers)
	 * @param relErrTol optional relative error tolerance (for variable step solvers)
	 */
	Integrator(
		const std::shared_ptr<System<STATE_DIM, SCALAR> >& system,
		const IntegrationType& intType = IntegrationType::EULERCT,
		const EventHandlerPtrVector& eventHandlers = EventHandlerPtrVector(0))
	:
	system_(system),
	observer_(eventHandlers),
	eventHandlers_(eventHandlers)
	{
		changeIntegrationType(intType);
		setupSystem();
	}

	/**
	 * @brief      Changes the integration type
	 *
	 * @param[in]  intType  The new integration type
	 */
	void changeIntegrationType(const IntegrationType& intType)
	{
		initializeCTSteppers(intType);
		initializeAdaptiveSteppers(intType);
		initializeODEIntSteppers(intType);
		if(!integratorStepper_)
			throw std::runtime_error("Unknown integration type");	
	}


	/**
	 * @brief      Sets the adaptive error tolerances
	 *
	 * @param[in]  absErrTol  The absolute error tolerance
	 * @param[in]  relErrTol  The relative error tolerance
	 */
	void setApadativeErrorTolerances(const SCALAR absErrTol, const SCALAR& relErrTol)
	{
		integratorStepper_->setAdaptiveErrorTolerances(absErrTol, relErrTol);
	}

	//! Equidistant integration based on number of time steps and step length
	/*!
	 * Integrates n steps forward from the current state recording the state and time trajectory.
	 * For a recording free version, see the function below.
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param numSteps number of steps to integrate forward
	 * @param dt step size (fixed also for variable step solvers)
	 * @param stateTrajectory state evolution over time
	 * @param timeTrajectory time trajectory corresponding to state trajectory
	 */
	void integrate_n_steps(
		StateVector<STATE_DIM, SCALAR>& state,
		const SCALAR& startTime,
		size_t numSteps,
		SCALAR dt,
		StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory,
		tpl::TimeArray<SCALAR>& timeTrajectory) 
	{
		reset();
		integratorStepper_->integrate_n_steps(observer_.observeWrap, systemFunction_, state, startTime, numSteps, dt);
		retrieveTrajectoriesFromObserver(stateTrajectory, timeTrajectory);	
	}

	//! Equidistant integration based on number of time steps and step length
	/*!
	 * Integrates n steps forward from the current state recording the state and time trajectory.
	 * For a recording free version, see the function below.
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param numSteps number of steps to integrate forward
	 * @param dt step size (fixed also for variable step solvers)
	 */
	void integrate_n_steps(
		StateVector<STATE_DIM, SCALAR>& state,
		const SCALAR& startTime,
		size_t numSteps,
		SCALAR dt) 
	{
		reset();
		integratorStepper_->integrate_n_steps(systemFunction_, state, startTime, numSteps, dt);
	}

	//! Equidistant integration based on initial and final time as well as step length
	/*!
	 * Integrates forward from the current state recording the state and time trajectory.
	 * For a recording free version, see the function below.
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param finalTime the final time of the integration
	 * @param dt step size (fixed also for variable step solvers)
	 * @param stateTrajectory state evolution over time
	 * @param timeTrajectory time trajectory corresponding to state trajectory
	 */
	void integrate_const(
		StateVector<STATE_DIM, SCALAR>& state,
		const SCALAR& startTime,
		const SCALAR& finalTime,
		SCALAR dt,
		StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory,
		tpl::TimeArray<SCALAR>& timeTrajectory) 
	{
		reset();
		integratorStepper_->integrate_const(observer_.observeWrap, systemFunction_, state, startTime, finalTime, dt);
		retrieveTrajectoriesFromObserver(stateTrajectory, timeTrajectory);
	}

	//! Equidistant integration based on initial and final time as well as step length
	/*!
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param finalTime the final time of the integration
	 * @param dt step size (fixed also for variable step solvers)
	 */
	void integrate_const(
			StateVector<STATE_DIM, SCALAR>& state,
			const SCALAR& startTime,
			const SCALAR& finalTime,
			SCALAR dt)
	{
		reset();
		integratorStepper_->integrate_const(systemFunction_, state, startTime, finalTime, dt);
	}

	//! integrate forward from an initial to a final time using an adaptive scheme
	/*!
	 * Integrates forward in time from an initial to a final time. If an adaptive stepper is used,
	 * the time step is adjusted according to the tolerances set in the constructor. If a fixed step
	 * integrator is used, this function will do fixed step integration.
	 *
	 * Records state and time evolution. For a recording free version see function below
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param finalTime the final time of the integration
	 * @param stateTrajectory state evolution over time
	 * @param timeTrajectory time trajectory corresponding to state trajectory
	 * @param dtInitial step size (initial guess, for fixed step integrators it is fixed)
	 */
	void integrate_adaptive(
			StateVector<STATE_DIM, SCALAR>& state,
			const SCALAR& startTime,
			const SCALAR& finalTime,
			StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory,
			tpl::TimeArray<SCALAR>& timeTrajectory,
			const SCALAR dtInitial = SCALAR(0.01)) 
	{
		reset();
		integratorStepper_->integrate_adaptive(observer_.observeWrap, systemFunction_, state, startTime, finalTime, dtInitial);
		retrieveTrajectoriesFromObserver(stateTrajectory, timeTrajectory);
		state = stateTrajectory.back();
	}

	//! integrate forward from an initial to a final time using an adaptive scheme
	/*!
	 * Integrates forward in time from an initial to a final time. If an adaptive stepper is used,
	 * the time step is adjusted according to the tolerances set in the constructor. If a fixed step
	 * integrator is used, this function will do fixed step integration.
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param startTime start time of the integration
	 * @param finalTime the final time of the integration
	 * @param dtInitial step size (initial guess, for fixed step integrators it is fixed)
	 */
	void integrate_adaptive(
			StateVector<STATE_DIM, SCALAR>& state,
			const SCALAR& startTime,
			const SCALAR& finalTime,
			SCALAR dtInitial = SCALAR(0.01)) 
	{
		reset();
		integratorStepper_->integrate_adaptive(systemFunction_, state, startTime, finalTime, dtInitial);
	}

	//! Integrate system using a given time trajectory
	/*!
	 * Integrates a system using a given time sequence
	 *
	 * \warning Overrides the initial state
	 *
	 * @param state initial state, contains the final state after integration
	 * @param timeTrajectory sequence of time stamps to perform the integration
	 * @param stateTrajectory the resulting state trajectory corresponding to the times provided
	 * @param dtInitial an initial guess for a time step (fixed for fixed step integrators)
	 */
	void integrate_times(
			StateVector<STATE_DIM, SCALAR>& state,
			const tpl::TimeArray<SCALAR>& timeTrajectory,
			StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory,
			SCALAR dtInitial = SCALAR(0.01))
    {

		reset();
		integratorStepper_->integrate_times(observer_.observeWrap, systemFunction_, state, timeTrajectory, dtInitial);
		retrieveStateVectorArrayFromObserver(stateTrajectory);
	}

private:

	/**
	 * @brief      Initializes the custom ct steppers
	 *
	 * @param[in]  intType  The integration type
	 */
	void initializeCTSteppers(const IntegrationType& intType)
	{
		switch(intType)
		{
			case EULERCT:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperEulerCT<Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperEulerCT<Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			case RK4CT:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperRK4CT<Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperRK4CT<Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			default:
				break;
		}	
	}

	/**
	 * @brief      Initializes the adaptive odeint steppers. The odeint steppers
	 *             only work for double currently
	 *
	 * @param[in]  intType  The integration type
	 *
	 */
	template<typename S = SCALAR>
	typename std::enable_if<std::is_same<S, double>::value, void>::type
	initializeAdaptiveSteppers(const IntegrationType& intType)
	{
		switch(intType)
		{
			case ODE45:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEIntControlled<internal::runge_kutta_dopri5_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEIntControlled<internal::runge_kutta_dopri5_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			case RK5VARIABLE:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEIntDenseOutput<internal::runge_kutta_dopri5_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEIntDenseOutput<internal::runge_kutta_dopri5_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());				
				break;
			}

			case BULIRSCHSTOER:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEInt<internal::bulirsch_stoer_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEInt<internal::bulirsch_stoer_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}
			default:
				break;
		}
	}

	template<typename S = SCALAR>
	typename std::enable_if<!std::is_same<S, double>::value, void>::type
	initializeAdaptiveSteppers(const IntegrationType& intType)
	{}

	template<typename S = SCALAR>
	typename std::enable_if<std::is_same<S, ADCGScalar>::value, void>::type
	initializeODEIntSteppers(const IntegrationType& intType)
	{}


	/**
	 * @brief      Initializes the ODEint fixed size steppers. Does not work for
	 *             ad types
	 *
	 * @param[in]  intType  The int type
	 *
	 */
	template<typename S = SCALAR>
	typename std::enable_if<!std::is_same<S, ADCGScalar>::value, void>::type
	initializeODEIntSteppers(const IntegrationType& intType)
	{
		switch(intType)
		{
			case EULER:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEInt<internal::euler_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEInt<internal::euler_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			case RK4:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEInt<internal::runge_kutta_4_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEInt<internal::runge_kutta_4_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			case MODIFIED_MIDPOINT:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEInt<internal::modified_midpoint_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEInt<internal::modified_midpoint_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());
				break;
			}

			case RK78:
			{
				integratorStepper_ = std::shared_ptr<internal::StepperODEInt<internal::runge_kutta_fehlberg78_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>>(
					new internal::StepperODEInt<internal::runge_kutta_fehlberg78_t<STATE_DIM, SCALAR>, Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>());

				break;
			}
			default:
				break;			
		}		
	}

	//! resets the observer
	void reset()
	{
		observer_.reset();
	}

	/**
	 * @brief      Retrieves the state and time trajectory from the observer.
	 *
	 * @param      stateTrajectory  The state trajectory
	 * @param      timeTrajectory   The time trajectory
	 */
	void retrieveTrajectoriesFromObserver(StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory, tpl::TimeArray<SCALAR>& timeTrajectory)
	{
		stateTrajectory.swap(observer_.stateTrajectory_);
		timeTrajectory.swap(observer_.timeTrajectory_);
	}
	/**
	 * @brief      Retrieves the state trajectory from the observer
	 *
	 * @param      stateTrajectory  The state trajectory
	 */
	void retrieveStateVectorArrayFromObserver(StateVectorArray<STATE_DIM, SCALAR>& stateTrajectory)
	{
		stateTrajectory.swap(observer_.stateTrajectory_);
	}


	//! sets up the lambda function
	void setupSystem() {

		systemFunction_ = [this]( const Eigen::Matrix<SCALAR, STATE_DIM, 1>& x, Eigen::Matrix<SCALAR, STATE_DIM, 1>& dxdt, SCALAR t) {
			const StateVector<STATE_DIM, SCALAR>& xState(static_cast<const StateVector<STATE_DIM, SCALAR>& >(x));
			StateVector<STATE_DIM, SCALAR>& dxdtState(static_cast<StateVector<STATE_DIM, SCALAR>& >(dxdt));
			system_->computeDynamics(xState, t, dxdtState);
		};

		reset();
	}

	std::shared_ptr<System<STATE_DIM, SCALAR> > system_; //! pointer to the system	
	std::function<void (const Eigen::Matrix<SCALAR, STATE_DIM, 1>&, Eigen::Matrix<SCALAR, STATE_DIM, 1>&, SCALAR)> systemFunction_; //! the system function to integrate
	std::shared_ptr<internal::StepperBase<Eigen::Matrix<SCALAR, STATE_DIM, 1>, SCALAR>> integratorStepper_;
	Observer<STATE_DIM, SCALAR> observer_; //! observer
	EventHandlerPtrVector eventHandlers_; //! vector of event handlers
};

}
}

#endif /* CT_CORE_INTEGRATOR_H_ */
