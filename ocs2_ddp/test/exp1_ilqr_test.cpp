/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include <ocs2_core/control/FeedforwardController.h>
#include <ocs2_oc/rollout/TimeTriggeredRollout.h>

#include <ocs2_ddp/ILQR.h>
#include <ocs2_oc/test/EXP1.h>

using namespace ocs2;

enum { STATE_DIM = 2, INPUT_DIM = 1 };

TEST(exp1_ilqr_test, exp1_ilqr_test) {
  ILQR_Settings ilqrSettings;
  ilqrSettings.ddpSettings_.displayInfo_ = true;
  ilqrSettings.ddpSettings_.displayShortSummary_ = true;
  ilqrSettings.ddpSettings_.absTolODE_ = 1e-10;
  ilqrSettings.ddpSettings_.relTolODE_ = 1e-7;
  ilqrSettings.ddpSettings_.maxNumStepsPerSecond_ = 10000;
  ilqrSettings.ddpSettings_.maxNumIterations_ = 30;
  ilqrSettings.ddpSettings_.minRelCost_ = 5e-4;
  ilqrSettings.ddpSettings_.checkNumericalStability_ = true;
  ilqrSettings.ddpSettings_.useFeedbackPolicy_ = false;
  ilqrSettings.ddpSettings_.debugPrintRollout_ = false;
  ilqrSettings.ddpSettings_.strategy_ = ddp_strategy::type::LINE_SEARCH;
  ilqrSettings.ddpSettings_.lineSearch_.minStepLength_ = 0.0001;

  Rollout_Settings rolloutSettings;
  rolloutSettings.absTolODE_ = 1e-11;
  rolloutSettings.relTolODE_ = 1e-8;
  rolloutSettings.maxNumStepsPerSecond_ = 10000;

  // event times
  std::vector<scalar_t> eventTimes{0.2262, 1.0176};
  std::vector<size_t> subsystemsSequence{0, 1, 2};
  std::shared_ptr<ModeScheduleManager> modeScheduleManagerPtr(new ModeScheduleManager({eventTimes, subsystemsSequence}));

  scalar_t startTime = 0.0;
  scalar_t finalTime = 3.0;

  // partitioning times
  std::vector<scalar_t> partitioningTimes;
  partitioningTimes.push_back(startTime);
  partitioningTimes.push_back(eventTimes[0]);
  partitioningTimes.push_back(eventTimes[1]);
  partitioningTimes.push_back(finalTime);

  vector_t initState(2);
  initState << 2.0, 3.0;

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  // system rollout
  EXP1_System systemDynamics(modeScheduleManagerPtr);
  TimeTriggeredRollout timeTriggeredRollout(systemDynamics, rolloutSettings);

  // system derivatives
  EXP1_SystemDerivative systemDerivative(modeScheduleManagerPtr);

  // system constraints
  ConstraintBase systemConstraint(STATE_DIM, INPUT_DIM);

  // system cost functions
  EXP1_CostFunction systemCostFunction(modeScheduleManagerPtr);

  // system operatingTrajectories
  vector_t stateOperatingPoint = vector_t::Zero(STATE_DIM);
  vector_t inputOperatingPoint = vector_t::Zero(INPUT_DIM);
  OperatingPoints operatingTrajectories(stateOperatingPoint, inputOperatingPoint);

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  // ILQR - single-threaded version
  ilqrSettings.ddpSettings_.nThreads_ = 1;
  ILQR ilqrST(STATE_DIM, INPUT_DIM, &timeTriggeredRollout, &systemDerivative, &systemConstraint, &systemCostFunction,
              &operatingTrajectories, ilqrSettings);
  ilqrST.setModeScheduleManager(modeScheduleManagerPtr);

  // ILQR - multi-threaded version
  ilqrSettings.ddpSettings_.nThreads_ = 3;
  ILQR ilqrMT(STATE_DIM, INPUT_DIM, &timeTriggeredRollout, &systemDerivative, &systemConstraint, &systemCostFunction,
              &operatingTrajectories, ilqrSettings);
  ilqrMT.setModeScheduleManager(modeScheduleManagerPtr);

  // run single_threaded core ILQR
  if (ilqrSettings.ddpSettings_.displayInfo_ || ilqrSettings.ddpSettings_.displayShortSummary_) {
    std::cerr << "\n>>> single-threaded ILQR" << std::endl;
  }
  ilqrST.run(startTime, initState, finalTime, partitioningTimes);

  // run multi-threaded ILQR
  if (ilqrSettings.ddpSettings_.displayInfo_ || ilqrSettings.ddpSettings_.displayShortSummary_) {
    std::cerr << "\n>>> multi-threaded ILQR" << std::endl;
  }
  ilqrMT.run(startTime, initState, finalTime, partitioningTimes);

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  // get solution
  auto solutionST = ilqrST.primalSolution(finalTime);
  auto solutionMT = ilqrMT.primalSolution(finalTime);

  // get performance indices
  auto performanceIndecesST = ilqrST.getPerformanceIndeces();
  auto performanceIndecesMT = ilqrMT.getPerformanceIndeces();

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  const scalar_t expectedCost = 5.4399;
  ASSERT_LT(fabs(performanceIndecesST.totalCost - expectedCost), 10 * ilqrSettings.ddpSettings_.minRelCost_)
      << "MESSAGE: single-threaded ILQR failed in the EXP1's cost test!";
  ASSERT_LT(fabs(performanceIndecesMT.totalCost - expectedCost), 10 * ilqrSettings.ddpSettings_.minRelCost_)
      << "MESSAGE: multi-threaded ILQR failed in the EXP1's cost test!";

  const scalar_t expectedISE1 = 0.0;
  ASSERT_LT(fabs(performanceIndecesST.stateInputEqConstraintISE - expectedISE1), 10 * ilqrSettings.ddpSettings_.constraintTolerance_)
      << "MESSAGE: single-threaded ILQR failed in the EXP1's type-1 constraint ISE test!";
  ASSERT_LT(fabs(performanceIndecesMT.stateInputEqConstraintISE - expectedISE1), 10 * ilqrSettings.ddpSettings_.constraintTolerance_)
      << "MESSAGE: multi-threaded ILQR failed in the EXP1's type-1 constraint ISE test!";

  const scalar_t expectedISE2 = 0.0;
  ASSERT_LT(fabs(performanceIndecesST.stateEqConstraintISE - expectedISE2), 10 * ilqrSettings.ddpSettings_.constraintTolerance_)
      << "MESSAGE: single-threaded ILQR failed in the EXP1's type-2 constraint ISE test!";
  ASSERT_LT(fabs(performanceIndecesMT.stateEqConstraintISE - expectedISE2), 10 * ilqrSettings.ddpSettings_.constraintTolerance_)
      << "MESSAGE: multi-threaded ILQR failed in the EXP1's type-2 constraint ISE test!";

  scalar_t ctrlFinalTime;
  if (ilqrSettings.ddpSettings_.useFeedbackPolicy_) {
    ctrlFinalTime = dynamic_cast<LinearController*>(solutionST.controllerPtr_.get())->timeStamp_.back();
  } else {
    ctrlFinalTime = dynamic_cast<FeedforwardController*>(solutionST.controllerPtr_.get())->timeStamp_.back();
  }
  ASSERT_DOUBLE_EQ(solutionST.timeTrajectory_.back(), finalTime) << "MESSAGE: ILQR_ST failed in policy final time of trajectory!";
  ASSERT_DOUBLE_EQ(ctrlFinalTime, finalTime) << "MESSAGE: ILQR_ST failed in policy final time of controller!";
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
