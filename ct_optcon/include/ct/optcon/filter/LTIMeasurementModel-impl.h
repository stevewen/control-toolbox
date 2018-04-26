/**********************************************************************************************************************
This file is part of the Control Toolbox (https://adrlab.bitbucket.io/ct), copyright by ETH Zurich, Google Inc.
Authors:  Michael Neunert, Markus Giftthaler, Markus Stäuble, Diego Pardo, Farbod Farshidian
Licensed under Apache2 license (see LICENSE file in main directory)
**********************************************************************************************************************/

#pragma once

namespace ct {
namespace optcon {

template <size_t OUTPUT_DIM, size_t STATE_DIM, typename SCALAR>
LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::LTIMeasurementModel()
{
    dHdx_.setZero();
    dHdw_.setIdentity();
}

template <size_t OUTPUT_DIM, size_t STATE_DIM, typename SCALAR>
LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::LTIMeasurementModel(
    const typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::output_state_matrix_t& C,
    const typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::output_matrix_t& dHdw)
    : dHdx_(C)
{
}
template <size_t OUTPUT_DIM, size_t STATE_DIM, typename SCALAR>
typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::output_vector_t
LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::computeMeasurement(
    const typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::state_vector_t& state,
    const Time_t& t)
{
    return dHdx_ * state;
}
template <size_t OUTPUT_DIM, size_t STATE_DIM, typename SCALAR>
typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::output_state_matrix_t
LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::computeDerivativeState(
    const typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::state_vector_t& state,
    const Time_t& t)
{
    return dHdx_;
}
template <size_t OUTPUT_DIM, size_t STATE_DIM, typename SCALAR>
typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::output_matrix_t
LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::computeDerivativeNoise(
    const typename LTIMeasurementModel<OUTPUT_DIM, STATE_DIM, SCALAR>::state_vector_t& state,
    const Time_t& t)
{
    return dHdw_;
}

}  // optcon
}  // ct
