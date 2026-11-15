/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2013 Yue Tian

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

/*! \file vannavolgainterpolation.hpp
    \brief Vanna/Volga interpolation between discrete points
*/

#ifndef quantlib_vanna_volga_interpolation_hpp
#define quantlib_vanna_volga_interpolation_hpp

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <algorithm>
#include <vector>

namespace QuantLib
{

    namespace detail
    {
        template <class I1, class I2>
        class VannaVolgaInterpolationImpl;
    }

    //! %Vanna Volga interpolation between discrete points
    /*! The method is an approximation for smiles that are neither too
        steep nor too skewed.  Measured from three months to ten years,
        at ATM volatilities from 1 % to 40 %, on smiles quoted at the
        25-delta put and call (forward delta) and the delta-neutral ATM
        strike, with the higher outer volatility at most twice the ATM
        one and its total volatility (the volatility times the square
        root of the time) below 1:

        - with both outer volatilities at least the ATM one, the values
          between the quotes stay within 3.2 % above the highest quote
          and 15.3 % below the lowest; past them the method didn't throw
          within 15 ATM standard deviations, and within 2 its values
          stayed above 0.71 times the nearer quote;
        - with one below the ATM one, the values past it keep falling,
          and the method fails there, from within 2.3 ATM standard
          deviations of it, unless the other lies far enough above the
          ATM volatility (#291): it held with the higher at 1.05, 1.1,
          1.25, 1.5, 1.75 or 2 times the ATM one and the lower at least
          0.97, 0.94, 0.89, 0.83, 0.78 or 0.75 times it, and failed in
          most markets with the lower 0.03 below that.  With the higher
          at most 1.25 times the ATM one, most of the markets that fail
          past the lower quote fail past the higher one too, from 0.4 to
          14.7 ATM standard deviations out.  Where it held, the values
          between the quotes stayed within 5.1 % above the highest quote
          and 12.1 % below the lowest, and past the lower quote they fell
          to 0.51 times it within 2 ATM standard deviations.

        Steeper smiles leave that range with no warning: with the
        higher outer volatility 2.5 times the ATM one and its total
        volatility still below 1, the values between the quotes reach
        1.41 times the highest quote, and beyond that 3.2 times it or
        0.16 times the lowest, and the method fails between the quotes
        in some markets.  With a higher outer total volatility above
        about 1.25, the method fails in most markets.  Where the
        method's call price leaves the bounds a call can take, value()
        throws, and says so.
    */
    class VannaVolgaInterpolation : public Interpolation
    {
      public:
        /*! \pre the three \f$ x \f$ values must be distinct, and the
                 second is the ATM strike, whose volatility must be
                 positive.  Each quote is weighted by the vega at the ATM
                 volatility, which must not vanish at any of the three
                 strikes: it underflows at about 38 ATM standard
                 deviations from the forward.
        */
        template <class I1, class I2>
        VannaVolgaInterpolation(const I1& xBegin,
                                const I1& xEnd,
                                const I2& yBegin,
                                Real spot,
                                DiscountFactor dDiscount,
                                DiscountFactor fDiscount,
                                Time T)
        {
            impl_ = ext::make_shared<detail::VannaVolgaInterpolationImpl<I1, I2>>(xBegin, xEnd, yBegin, spot, dDiscount,
                                                                                  fDiscount, T);
            impl_->update();
        }
    };

    //! %VannaVolga-interpolation factory and traits
    class VannaVolga
    {
      public:
        VannaVolga(Real spot, DiscountFactor dDiscount, DiscountFactor fDiscount, Time T)
        : spot_(spot), dDiscount_(dDiscount), fDiscount_(fDiscount), T_(T)
        {
        }
        template <class I1, class I2>
        Interpolation interpolate(const I1& xBegin, const I1& xEnd, const I2& yBegin) const
        {
            return VannaVolgaInterpolation(xBegin, xEnd, yBegin, spot_, dDiscount_, fDiscount_, T_);
        }
        static const Size requiredPoints = 3;

      private:
        Real spot_;
        DiscountFactor dDiscount_;
        DiscountFactor fDiscount_;
        Time T_;
    };

    namespace detail
    {

        template <class I1, class I2>
        class VannaVolgaInterpolationImpl final : public Interpolation::templateImpl<I1, I2>
        {
          public:
            VannaVolgaInterpolationImpl(const I1& xBegin,
                                        const I1& xEnd,
                                        const I2& yBegin,
                                        Real spot,
                                        DiscountFactor dDiscount,
                                        DiscountFactor fDiscount,
                                        Time T)
            : Interpolation::templateImpl<I1, I2>(xBegin, xEnd, yBegin, VannaVolga::requiredPoints), spot_(spot),
              dDiscount_(dDiscount), fDiscount_(fDiscount), T_(T)
            {
                QL_REQUIRE(this->xEnd_ - this->xBegin_ == 3,
                           "Vanna Volga Interpolator only interpolates 3 volatilities in strike space");
            }
            void update() override
            {
                // atmVol should be the second vol
                atmVol_ = this->yBegin_[1];
                fwd_ = spot_ * fDiscount_ / dDiscount_;
                // value() weights each quote by the vega at k over the vega at its strike, at the ATM volatility, and
                // divides by the logs of the strikes' ratios.
                QL_REQUIRE(atmVol_ > 0.0, "Vanna/Volga interpolation needs a positive ATM volatility, not " << atmVol_);
                QL_REQUIRE(this->xBegin_[0] != this->xBegin_[1] && this->xBegin_[0] != this->xBegin_[2] &&
                               this->xBegin_[1] != this->xBegin_[2],
                           "Vanna/Volga interpolation needs three distinct strikes, not "
                               << this->xBegin_[0] << ", " << this->xBegin_[1] << " and " << this->xBegin_[2]);
                premiaBS.clear();
                premiaMKT.clear();
                vegas.clear();
                for (Size i = 0; i < 3; i++)
                {
                    premiaBS.push_back(
                        blackFormula(Option::Call, this->xBegin_[i], fwd_, atmVol_ * std::sqrt(T_), dDiscount_));
                    premiaMKT.push_back(blackFormula(Option::Call, this->xBegin_[i], fwd_,
                                                     this->yBegin_[i] * std::sqrt(T_), dDiscount_));
                    vegas.push_back(vega(this->xBegin_[i]));
                    QL_REQUIRE(vegas.back() > 0.0,
                               "Vanna/Volga interpolation: the vega at the strike "
                                   << this->xBegin_[i] << " is 0 at the ATM volatility " << atmVol_
                                   << ", as the strike lies "
                                   << std::fabs(std::log(fwd_ / this->xBegin_[i])) / (atmVol_ * std::sqrt(T_))
                                   << " ATM standard deviations from the forward " << fwd_);
                }
            }
            Real value(Real k) const override
            {
                Real x1 =
                    vega(k) / vegas[0] * (std::log(this->xBegin_[1] / k) * std::log(this->xBegin_[2] / k)) /
                    (std::log(this->xBegin_[1] / this->xBegin_[0]) * std::log(this->xBegin_[2] / this->xBegin_[0]));
                Real x2 =
                    vega(k) / vegas[1] * (std::log(k / this->xBegin_[0]) * std::log(this->xBegin_[2] / k)) /
                    (std::log(this->xBegin_[1] / this->xBegin_[0]) * std::log(this->xBegin_[2] / this->xBegin_[1]));
                Real x3 =
                    vega(k) / vegas[2] * (std::log(k / this->xBegin_[0]) * std::log(k / this->xBegin_[1])) /
                    (std::log(this->xBegin_[2] / this->xBegin_[0]) * std::log(this->xBegin_[2] / this->xBegin_[1]));

                Real cBS = blackFormula(Option::Call, k, fwd_, atmVol_ * std::sqrt(T_), dDiscount_);
                Real c = cBS + x1 * (premiaMKT[0] - premiaBS[0]) + x2 * (premiaMKT[1] - premiaBS[1]) +
                         x3 * (premiaMKT[2] - premiaBS[2]);
                try
                {
                    Real std = blackFormulaImpliedStdDev(Option::Call, k, fwd_, c, dDiscount_);
                    return std / sqrt(T_);
                }
                catch (const std::exception&)
                {
                    // Where the method doesn't hold, its price can leave the bounds a call can take, and the
                    // inversion then blamed the price (#242).  Say why instead; any other failure keeps its message.
                    const Real lower = dDiscount_ * std::max(fwd_ - k, Real(0.0)), upper = dDiscount_ * fwd_;
                    if (c >= lower && c <= upper)
                        throw;
                    const Real atmStdDev = atmVol_ * std::sqrt(T_);
                    QL_FAIL("Vanna/Volga interpolation can't price the strike "
                            << k << ": the method's call price " << c << " lies outside [" << lower << ", " << upper
                            << "], the bounds of a call, as the method doesn't hold for this smile: its quotes at "
                            << this->xBegin_[0] << " and " << this->xBegin_[2] << " lie "
                            << std::fabs(std::log(this->xBegin_[0] / this->xBegin_[1])) / atmStdDev << " and "
                            << std::fabs(std::log(this->xBegin_[2] / this->xBegin_[1])) / atmStdDev
                            << " ATM standard deviations from the ATM strike " << this->xBegin_[1]
                            << ", with volatilities " << this->yBegin_[0] / atmVol_ << " and "
                            << this->yBegin_[2] / atmVol_ << " times the ATM one");
                }
            }
            Real primitive(Real) const override { QL_FAIL("Vanna Volga primitive not implemented"); }
            Real derivative(Real) const override { QL_FAIL("Vanna Volga derivative not implemented"); }
            Real secondDerivative(Real) const override { QL_FAIL("Vanna Volga secondDerivative not implemented"); }

          private:
            std::vector<Real> premiaBS;
            std::vector<Real> premiaMKT;
            std::vector<Real> vegas;
            Real atmVol_;
            Real spot_;
            Real fwd_;
            DiscountFactor dDiscount_;
            DiscountFactor fDiscount_;
            Time T_;

            Real vega(Real k) const
            {
                Real d1 = (std::log(fwd_ / k) + 0.5 * std::pow(atmVol_, 2.0) * T_) / (atmVol_ * std::sqrt(T_));
                NormalDistribution norm;
                return spot_ * dDiscount_ * std::sqrt(T_) * norm(d1);
            }
        };

    }

}

#endif
