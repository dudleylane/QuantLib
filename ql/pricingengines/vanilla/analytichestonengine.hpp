/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2005, 2008 Klaus Spanderen
 Copyright (C) 2007 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file analytichestonengine.hpp
    \brief analytic Heston-model engine
*/

#ifndef quantlib_analytic_heston_engine_hpp
#define quantlib_analytic_heston_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>
#include <ql/utilities/null.hpp>
#include <complex>
#include <functional>

namespace QuantLib
{

    namespace detail
    {

        // At rho = -1 or 1, the slope tan(phi) of the line u (1 + i tan(phi)) - i / 2 along which the
        // characteristic function is integrated, from r = rho - sigma freq / w alone, freq = log(F / K) and
        // w = v + kappa theta t for the variance v and the term t, held where the line tilts against freq;
        // vT is the variance expected over the term (#239).  AnalyticHestonEngine and
        // AnalyticHestonForwardEuropeanEngine both take it, so it is declared here.  The derivation is at its
        // definition, in analytichestonengine.cpp.
        Real hestonUnitCorrelationTanPhi(Real r, Real freq, Real vT);

        // At rho = -1 or 1, the bound e^{P - a sqrt(u)} on the characteristic function along that line, from
        // w = v + kappa theta t and its slope tanPhi (#239).
        struct HestonUnitCorrelationBound
        {
            Real a, P;
        };

        HestonUnitCorrelationBound
        hestonUnitCorrelationBound(Real rho, Real kappa, Real theta, Real sigma, Real w, Real tanPhi);

        // Andersen and Piterbarg's limit read off the characteristic function past u0, the closed form's root, where
        // the criterion there is still above epsilon (#340); logModulus(u) is log |phi(u - i (alpha + 1))|.
        // AnalyticHestonEngine and AnalyticPTDHestonEngine both take it (#344), so it is declared here. The criterion
        // and what the read does where it can't go on are at its definition, in analytichestonengine.cpp.
        Real andersenPiterbargLimitFromChF(Real u0, Real epsilon, const std::function<Real(Real)>& logModulus);

        // log psi(z), psi the Heston characteristic function of log(S_t / F), in Andersen and Lake's form, where g - D
        // isn't taken as a difference where it cancels; at sigma <= 1e-100, sigma = 0's limit (#348, #355).
        // AnalyticHestonEngine::lnChF, its Gatheral and BranchCorrection forms, and COSHestonEngine::chF take it
        // (#349, #350).
        std::complex<Real>
        hestonLnChF(Real kappa, Real theta, Real sigma, Real rho, Real v0, const std::complex<Real>& z, Time t);

    }

    //! analytic Heston-model engine based on Fourier transform

    /*! Integration detail:
        Two algebraically equivalent formulations of the complex
        logarithm of the Heston model exist. Gatherals [2005]
        (also Duffie, Pan and Singleton [2000], and Schoutens,
        Simons and Tistaert[2004]) version does not cause
        discoutinuities whereas the original version (e.g. Heston [1993])
        needs some sort of "branch correction" to work properly.
        Both Gatheral and BranchCorrection now take the characteristic
        function in Andersen and Lake's form, as lnChF does, which
        neither cancels at small sigma nor crosses the branch cut, so
        the two give the same prices (#349, #350). The constructor still
        refuses BranchCorrection with the adaptive rules, as it did when
        the form tracked its branch from one node to the next; Gatheral
        takes them.

        At rho = -1 or 1 the decay rate c_inf is 0, and the integrand can
        decay as slowly as e^{-a sqrt(u)} (#239). There Gauss-Lobatto, and so
        the (model, relTolerance, maxEvaluations) constructor, takes its
        limit from that decay along the angled contour, with AngledContour,
        AngledContourNoCV or OptimalCV, which takes AngledContour there. It
        integrates [0, 1] and the rest apart, or the whole where [0, 1]
        doesn't converge, so it may take up to about twice its
        maxEvaluations. With Gauss-Lobatto the Andersen-Piterbarg and
        AsymptoticChF control variates refuse rho = -1 and 1. The other rules
        that take a limit refuse it with any control variate, as do
        Gauss-Legendre and the Gauss-Chebyshev rules, which map their nodes
        through c_inf. Gauss-Laguerre and exp-sinh take no limit and price as
        before: there AsymptoticChF's control variate, which OptimalCV takes
        past 0.15 years, vanishes, and Gauss-Laguerre can be far off at short
        maturities (#300). A limit past 5e8 is refused, as near
        kappa = sigma / 2 at rho = 1 with the strike near F e^{-w / sigma},
        w = v0 + kappa theta t, and minutes from expiry or with v0 near 0.
        This path is taken at rho = -1 and 1 exactly: just inside them the
        engine takes the path it takes wherever |rho| < 1, whose limit grows
        as 1 / c_inf, and it can be far less accurate there (#296).

        AsymptoticChF integrates phi less its asymptotic at large u and
        adds back the asymptotic's closed form, which carries e^{psi} with
            Re psi = [(kappa - rho sigma / 2)(v0 + kappa theta t)
                      + kappa theta log(4 (1 - rho^2))] / sigma^2,
        so a rule's error on what is left is multiplied by about
        e^{Re psi}. OptimalCV takes AsymptoticChF only where Re psi is
        below 0.1, past 0.15 years and with c_inf below 0.15, and an
        explicit AsymptoticChF is refused where Re psi is 0.1 or more, in
        calculate(), priceVanillaPayoff(), the deprecated doCalculation()
        and ExponentialFittingHestonEngine alike (#342). There calls on a
        spot of 100, three months and longer, came out off by up to 2.1e-6
        below Re psi 1 and 0.012 below 10 with Gauss-Laguerre at 128
        nodes, and by up to 4.4 below 10 with
        ExponentialFittingHestonEngine, which is off below the bound too
        (#373); far more beyond, with nothing to say so. Gauss-Kronrod and
        exp-sinh at 1e-8 held to Re psi of about 10 and 2, and
        Gauss-Lobatto ran out of evaluations rather than come out off, but
        no higher bound holds for every rule, so they are refused with the
        rest. Re psi is 0.1 or more on most parameters: 95 % of a grid of
        kappa 0.5 to 3, 3 months to 2 years, v0 0.01 to 0.16, sigma 0.3 to
        0.8 and rho -0.7 to 0.3. OptimalCV takes AngledContour there.
    */

    /*! References:

        Heston, Steven L., 1993. A Closed-Form Solution for Options
        with Stochastic Volatility with Applications to Bond and
        Currency Options.  The review of Financial Studies, Volume 6,
        Issue 2, 327-343.

        A. Sepp, Pricing European-Style Options under Jump Diffusion
        Processes with Stochastic Volatility: Applications of Fourier
        Transform (<http://math.ut.ee/~spartak/papers/stochjumpvols.pdf>)

        R. Lord and C. Kahl, Why the rotation count algorithm works,
        http://papers.ssrn.com/sol3/papers.cfm?abstract_id=921335

        H. Albrecher, P. Mayer, W.Schoutens and J. Tistaert,
        The Little Heston Trap, http://www.schoutens.be/HestonTrap.pdf

        J. Gatheral, The Volatility Surface: A Practitioner's Guide,
        Wiley Finance

        F. Le Floc'h, Fourier Integration and Stochastic Volatility
        Calibration,
        https://papers.ssrn.com/sol3/papers.cfm?abstract_id=2362968

        L. Andersen, and V. Piterbarg, 2010,
        Interest Rate Modeling, Volume I: Foundations and Vanilla Models,
        Atlantic Financial Press London.

        L. Andersen and M. Lake, 2018
        Robust High-Precision Option Pricing by Fourier Transforms:
        Contour Deformations and Double-Exponential Quadrature,
        https://papers.ssrn.com/sol3/papers.cfm?abstract_id=3231626

        \ingroup vanillaengines

        \test the correctness of the returned value is tested by
              reproducing results available in web/literature
              and comparison with Black pricing.
    */
    class AnalyticHestonEngine
    : public GenericModelEngine<HestonModel, VanillaOption::arguments, VanillaOption::results>
    {
      public:
        class Integration;
        class OptimalAlpha;
        class AP_Helper;

        enum ComplexLogFormula
        {
            // Gatheral form of characteristic function w/o control variate
            Gatheral,
            // Heston's original form with a branch correction until #350, now the Gatheral form; w/o control variate
            BranchCorrection,
            // Gatheral form with Andersen-Piterbarg control variate
            AndersenPiterbarg,
            // same as AndersenPiterbarg, but a slightly better control variate
            AndersenPiterbargOptCV,
            // Gatheral form with asymptotic expansion of the characteristic function as control variate
            // https://hpcquantlib.wordpress.com/2020/08/30/a-novel-control-variate-for-the-heston-model
            // Refused where Re psi, whose exponential the control variate carries, is 0.1 or more (#342)
            AsymptoticChF,
            // angled contour shift integral with control variate
            AngledContour,
            // angled contour shift integral w/o control variate
            AngledContourNoCV,
            // auto selection of best control variate algorithm from above
            OptimalCV
        };

        // Simple to use constructor: Using adaptive
        // Gauss-Lobatto integration and the OptimalCV form, with an
        // Andersen-Piterbarg epsilon of 1e-40.
        // Be aware: using a too large number for maxEvaluations might result
        // in a stack overflow as the Lobatto integration is a recursive
        // algorithm. It takes Integration::gaussLobatto's minimum depth of 2,
        // so maxEvaluations must be above 225 (#341).
        AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model, Real relTolerance, Size maxEvaluations);

        // Constructor using Laguerre integration
        // and Gatheral's version of complex log.
        AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model, Size integrationOrder = 144);

        // Constructor giving full control
        // over the Fourier integration algorithm
        AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model,
                             ComplexLogFormula cpxLog,
                             const Integration& itg,
                             Real andersenPiterbargEpsilon = 1e-25,
                             Real alpha = -0.5);

        void calculate() const override;

        // normalized characteristic function
        std::complex<Real> chF(const std::complex<Real>& z, Time t) const;
        std::complex<Real> lnChF(const std::complex<Real>& z, Time t) const;

        Size numberOfEvaluations() const;

        [[deprecated("Use AnalyticHestonEngine::priceVanillaPayoff instead.")]]
        static void doCalculation(Real riskFreeDiscount,
                                  Real dividendDiscount,
                                  Real spotPrice,
                                  Real strikePrice,
                                  Real term,
                                  Real kappa,
                                  Real theta,
                                  Real sigma,
                                  Real v0,
                                  Real rho,
                                  const TypePayoff& type,
                                  const Integration& integration,
                                  ComplexLogFormula cpxLog,
                                  const AnalyticHestonEngine* enginePtr,
                                  Real& value,
                                  Size& evaluations);

        Real priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff, const Date& maturity) const;

        Real priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff, Time maturity) const;

        // AsymptoticChF past 0.15 years where c_inf is below 0.15 and Re psi below 0.1, the bound at which an explicit
        // AsymptoticChF is refused (#342); AngledContour elsewhere
        static ComplexLogFormula optimalControlVariate(Time t, Real v0, Real kappa, Real theta, Real sigma, Real rho);

      protected:
        // call back for extended stochastic volatility
        // plus jump diffusion engines like bates model
        //
        // Where a rule takes phi = 0 as a node (Gauss-Lobatto, Simpson's and the
        // trapezoid rule, and the discrete Simpson and trapezoid rules), the Gatheral
        // and BranchCorrection forms take the integrand's limit there. Its part from
        // this term is the slope of the term's imaginary part at 0, taken by a central
        // difference at phi = 1e-8 and -1e-8 (#362), so the term is also called with a
        // negative phi; its imaginary part should be odd in phi, as a characteristic
        // function's logarithm's is.
        virtual std::complex<Real> addOnTerm(Real phi, Time t, Size j) const;

      private:
        class Fj_Helper;

        Real priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff, Time maturity, Real fwd) const;


        mutable Size evaluations_;
        const ComplexLogFormula cpxLog_;
        const ext::shared_ptr<Integration> integration_;
        const Real andersenPiterbargEpsilon_, alpha_;
    };


    class AnalyticHestonEngine::Integration
    {
      public:
        // non adaptive integration algorithms based on Gaussian quadrature
        static Integration gaussLaguerre(Size integrationOrder = 128);
        static Integration gaussLegendre(Size integrationOrder = 128);
        static Integration gaussChebyshev(Size integrationOrder = 128);
        static Integration gaussChebyshev2nd(Size integrationOrder = 128);

        // Gatheral's version has to be used for an adaptive integration
        // algorithm .Be aware: using a too large number for maxEvaluations might
        // result in a stack overflow as the these integrations are based on
        // recursive algorithms.
        // The whole range is split minimumDepth levels deep before any panel is accepted, as a wide panel whose two
        // estimates agreed while both were off was taken for the integral there (#341). At 2, the default, an
        // integral takes at least 230 evaluations, and maxEvaluations must be above 225; at 0 the rule is
        // GaussLobattoIntegral's default.
        static Integration gaussLobatto(Real relTolerance,
                                        Real absTolerance,
                                        Size maxEvaluations = 1000,
                                        bool useConvergenceEstimate = false,
                                        Size minimumDepth = 2);

        // usually these routines have a poor convergence behavior.
        static Integration gaussKronrod(Real absTolerance, Size maxEvaluations = 1000);
        // Simpson's and the trapezoid rule halve their intervals until two estimates in a row agree to
        // absTolerance, and stop no earlier than the 7th estimate. maxIterations bounds the estimates, and the n-th
        // rests on 2^(n-1) + 1 evaluations: the default allows 8,388,609 an integral before "max number of
        // iterations reached" is thrown, and each step up doubles that. Calls can need more within about 1e-8 of
        // rho = -1 or 1, with Gatheral's formula from about |rho| = 0.99, and with Gatheral's formula and the
        // trapezoid rule at a tolerance of 1e-12 a month or less from expiry: pass a larger bound there. Until #294
        // the bound was named maxEvaluations, counted the same way and defaulted to 1000, so a call whose
        // estimates never agreed didn't return.
        static Integration simpson(Real absTolerance, Size maxIterations = 24);
        static Integration trapezoid(Real absTolerance, Size maxIterations = 24);
        static Integration discreteSimpson(Size evaluation = 1000);
        static Integration discreteTrapezoid(Size evaluation = 1000);
        static Integration expSinh(Real relTolerance = 1e-8);

        /* The larger of the roots where e^{-c_inf u} / u and the Gaussian e^{-v t u^2 / 2} / u fall to epsilon.
           AnalyticHestonEngine passes the average variance theta + (v0 - theta) (1 - e^{-kappa t}) / (kappa t) as v,
           the variance the characteristic function decays at as a Gaussian below u of about 1 / (sigma t); it passed v0
           until #339. AnalyticPTDHestonEngine passes its control variate's variance, the same expression on the plain
           means of kappa and theta over its grid, so that the root covers the control variate's decay; it passed v0
           until #344. On piecewise parameters that average needn't be the variance the characteristic function decays
           at, and c_inf, the decay rate as u goes to infinity, can overstate the decay over the range the integral
           needs.
           Both are asymptotics of Andersen and Piterbarg's criterion |phi(u - i (alpha + 1))| / u, so the root can stop
           short of where that falls to epsilon: e^{-c_inf u} leaves out the asymptotic's constant e^{Re psi} (#340),
           and between the Gaussian and the exponential regimes phi follows neither (#347).
           Both engines therefore read the limit off phi where the criterion at the root is still above epsilon, and
           above QL_EPSILON (#340, and #344 for AnalyticPTDHestonEngine): the read doubles past the root until the
           criterion is below epsilon, and solves between the last two points. It keeps the root where log |phi| isn't
           finite at a doubling point, or where the criterion hasn't fallen below epsilon by 2^64 times the root. The
           criterion takes phi on the line AndersenPiterbarg and AndersenPiterbargOptCV integrate along; for the forms
           that tilt that line, and for AsymptoticChF, which integrates phi less its asymptotic, it stands in for their
           integrand's decay, as the root did. At AnalyticHestonEngine's default andersenPiterbargEpsilon of 1e-25 the
           criterion at the root was below QL_EPSILON wherever #340 measured, so the limit there is as before;
           AnalyticPTDHestonEngine's default is 1e-8. */
        static Real andersenPiterbargIntegrationLimit(Real c_inf, Real epsilon, Real v, Real t);

        Real calculate(Real c_inf,
                       const std::function<Real(Real)>& f,
                       const std::function<Real()>& maxBound = {},
                       Real scaling = 1.0) const;

        Real calculate(Real c_inf, const std::function<Real(Real)>& f, Real maxBound) const;

        // where c_inf is 0, Gauss-Lobatto's two integrations together: up to about twice maxEvaluations (#239)
        Size numberOfEvaluations() const;
        bool isAdaptiveIntegration() const;

      private:
        // which contour and control variate it takes at rho = -1 or 1 depends on the engine's rule (#239)
        friend class AnalyticHestonEngine;
        friend class AnalyticHestonEngine::AP_Helper;

        // at rho = -1 or 1, where c_inf is 0, Gauss-Lobatto takes #239's contour, limit and control variate
        bool takesUnitCorrelationPath(Real rho) const;

        enum Algorithm
        {
            GaussLobatto,
            GaussKronrod,
            Simpson,
            Trapezoid,
            DiscreteTrapezoid,
            DiscreteSimpson,
            GaussLaguerre,
            GaussLegendre,
            GaussChebyshev,
            GaussChebyshev2nd,
            ExpSinh
        };

        Integration(Algorithm intAlgo, ext::shared_ptr<GaussianQuadrature> quadrature);

        Integration(Algorithm intAlgo, ext::shared_ptr<Integrator> integrator);

        Real unitCorrelationIntegral(const std::function<Real(Real)>& f, Real maxBound) const;

        const Algorithm intAlgo_;
        const ext::shared_ptr<Integrator> integrator_;
        const ext::shared_ptr<GaussianQuadrature> gaussianQuadrature_;
        // the evaluations unitCorrelationIntegral spent on [0, 1] before its second integration (#239)
        mutable Size headEvaluations_ = 0;
    };

    class AnalyticHestonEngine::AP_Helper
    {
      public:
        AP_Helper(Time term,
                  Real fwd,
                  Real strike,
                  ComplexLogFormula cpxLog,
                  const AnalyticHestonEngine* enginePtr,
                  Real alpha = -0.5);

        Real operator()(Real u) const;
        Real controlVariateValue() const;

      private:
        const Time term_;
        const Real fwd_, strike_, freq_;
        const ComplexLogFormula cpxLog_;
        const AnalyticHestonEngine* const enginePtr_;
        const Real alpha_, s_alpha_;
        Real vAvg_, tanPhi_;
        std::complex<Real> phi_, psi_;
        bool unitCorrelation_;
    };


    class AnalyticHestonEngine::OptimalAlpha
    {
      public:
        OptimalAlpha(Time t, const AnalyticHestonEngine* enginePtr);

        Real operator()(Real strike) const;
        std::pair<Real, Real> alphaGreaterZero(Real strike) const;
        std::pair<Real, Real> alphaSmallerMinusOne(Real strike) const;

        Size numberOfEvaluations() const;
        Real M(Real k) const;
        Real k(Real x, Integer sgn) const;
        Real alphaMin(Real strike) const;
        Real alphaMax(Real strike) const;

      private:
        std::pair<Real, Real> findMinima(Real lower, Real upper, Real strike) const;

        const Real t_, fwd_, kappa_, theta_, sigma_, rho_;

        const Real eps_;

        const AnalyticHestonEngine* const enginePtr_;
        Real km_, kp_;
        mutable Size evaluations_ = 0;
    };


    inline std::complex<Real> AnalyticHestonEngine::addOnTerm(Real, Time, Size) const
    {
        return std::complex<Real>(0, 0);
    }
}

#endif
