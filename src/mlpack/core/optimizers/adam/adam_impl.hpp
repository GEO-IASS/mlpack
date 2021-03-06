/**
 * @file adam_impl.hpp
 * @author Ryan Curtin
 * @author Vasanth Kalingeri
 * @author Marcus Edel
 * @author Vivek Pal
 *
 * Implementation of the Adam and AdaMax optimizer.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_CORE_OPTIMIZERS_ADAM_ADAM_IMPL_HPP
#define MLPACK_CORE_OPTIMIZERS_ADAM_ADAM_IMPL_HPP

// In case it hasn't been included yet.
#include "adam.hpp"

namespace mlpack {
namespace optimization {

template<typename DecomposableFunctionType>
Adam<DecomposableFunctionType>::Adam(DecomposableFunctionType& function,
                                     const double stepSize,
                                     const double beta1,
                                     const double beta2,
                                     const double eps,
                                     const size_t maxIterations,
                                     const double tolerance,
                                     const bool shuffle,
                                     const bool adaMax) :
    function(function),
    stepSize(stepSize),
    beta1(beta1),
    beta2(beta2),
    eps(eps),
    maxIterations(maxIterations),
    tolerance(tolerance),
    shuffle(shuffle),
    adaMax(adaMax)
{ /* Nothing to do. */ }

//! Optimize the function (minimize).
template<typename DecomposableFunctionType>
double Adam<DecomposableFunctionType>::Optimize(arma::mat& iterate)
{
  // Find the number of functions to use.
  const size_t numFunctions = function.NumFunctions();

  // This is used only if shuffle is true.
  arma::Col<size_t> visitationOrder;
  if (shuffle)
    visitationOrder = arma::shuffle(arma::linspace<arma::Col<size_t>>(0,
        (numFunctions - 1), numFunctions));

  // To keep track of where we are and how things are going.
  size_t currentFunction = 0;
  double overallObjective = 0;
  double lastObjective = DBL_MAX;

  // Calculate the first objective function.
  for (size_t i = 0; i < numFunctions; ++i)
    overallObjective += function.Evaluate(iterate, i);

  // Now iterate!
  arma::mat gradient(iterate.n_rows, iterate.n_cols);

  // Exponential moving average of gradient values.
  arma::mat m = arma::zeros<arma::mat>(iterate.n_rows, iterate.n_cols);

  /**
   * Initialize  either the exponentially weighted infinity norm for AdaMax
   * optimizer (u) or exponential moving average of squared gradient values
   * for Adam optimizer (v).
   */
  arma::mat u, v;
  if (adaMax)
  {
    u = arma::zeros<arma::mat>(iterate.n_rows, iterate.n_cols);
  }
  else
  {
    v = arma::zeros<arma::mat>(iterate.n_rows, iterate.n_cols);
  }

  for (size_t i = 1; i != maxIterations; ++i, ++currentFunction)
  {
    // Is this iteration the start of a sequence?
    if ((currentFunction % numFunctions) == 0)
    {
      // Output current objective function.
      Log::Info << "Adam: iteration " << i << ", objective " << overallObjective
          << "." << std::endl;

      if (std::isnan(overallObjective) || std::isinf(overallObjective))
      {
        Log::Warn << "Adam: converged to " << overallObjective
            << "; terminating with failure. Try a smaller step size?"
            << std::endl;
        return overallObjective;
      }

      if (std::abs(lastObjective - overallObjective) < tolerance)
      {
        Log::Info << "Adam: minimized within tolerance " << tolerance << "; "
            << "terminating optimization." << std::endl;
        return overallObjective;
      }

      // Reset the counter variables.
      lastObjective = overallObjective;
      overallObjective = 0;
      currentFunction = 0;

      if (shuffle) // Determine order of visitation.
        visitationOrder = arma::shuffle(visitationOrder);
    }

    // Evaluate the gradient for this iteration.
    if (shuffle)
      function.Gradient(iterate, visitationOrder[currentFunction], gradient);
    else
      function.Gradient(iterate, currentFunction, gradient);

    // And update the iterate.
    m *= beta1;
    m += (1 - beta1) * gradient;

    if (adaMax)
    {
      // Update the exponentially weighted infinity norm.
      u *= beta2;
      u = arma::max(u, arma::abs(gradient));
    }
    else
    {
      v *= beta2;
      v += (1 - beta2) * (gradient % gradient);
    }

    const double biasCorrection1 = 1.0 - std::pow(beta1, (double) i);
    const double biasCorrection2 = 1.0 - std::pow(beta2, (double) i);

    if (adaMax)
    {
      if (biasCorrection1 != 0.0)
        iterate -= (stepSize / biasCorrection1 * m / (u + eps));
    }
    else
    {
      /**
       * It should be noted that the term, m / (arma::sqrt(v) + eps), in the
       * following expression is an approximation of the following actual term;
       * m / (arma::sqrt(v) + (arma::sqrt(biasCorrection2) * eps).
       */
      iterate -= (stepSize * std::sqrt(biasCorrection2) / biasCorrection1) *
                  m / (arma::sqrt(v) + eps);
    }

    // Now add that to the overall objective function.
    if (shuffle)
      overallObjective += function.Evaluate(iterate,
          visitationOrder[currentFunction]);
    else
      overallObjective += function.Evaluate(iterate, currentFunction);
  }

  Log::Info << "Adam: maximum iterations (" << maxIterations << ") reached; "
      << "terminating optimization." << std::endl;
  // Calculate final objective.
  overallObjective = 0;
  for (size_t i = 0; i < numFunctions; ++i)
    overallObjective += function.Evaluate(iterate, i);
  return overallObjective;
}

} // namespace optimization
} // namespace mlpack

#endif
