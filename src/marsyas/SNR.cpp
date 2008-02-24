/*
** Copyright (C) 1998-2006 George Tzanetakis <gtzan@cs.uvic.ca>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "SNR.h"

using namespace std;
using namespace Marsyas;

SNR::SNR(string name):MarSystem("SNR", name)
{
	addControls();
}

SNR::SNR(const SNR& a) : MarSystem(a)
{
	ctrl_mode_ = getctrl("mrs_string/mode");
}


SNR::~SNR()
{
}

MarSystem*
SNR::clone() const
{
	return new SNR(*this);
}

void
SNR::addControls()
{
  addctrl("mrs_string/mode", "standard", ctrl_mode_);
}

void
SNR::myUpdate(MarControlPtr sender)
{
  (void) sender;
  MRSDIAG("SNR.cpp - SNR:myUpdate");
  ctrl_onSamples_->setValue((mrs_natural)1, NOUPDATE);
  ctrl_onObservations_->setValue((mrs_natural)1, NOUPDATE);
  ctrl_osrate_->setValue(ctrl_israte_, NOUPDATE);
  ctrl_onObsNames_->setValue("SNR_" + ctrl_inObsNames_->to<mrs_string>() , NOUPDATE);
}

void
SNR::myProcess(realvec& in, realvec& out)
{
  nsum_ = 0.0;
  dsum_ = 0.0;
  const mrs_string& mode = ctrl_mode_->to<mrs_string>();

  
  
  for (t = 0; t < inSamples_; t++)
    {
      nsum_ += (in(0,t) * in(0,t));
      diff_ = (in(0,t) - in(1,t));
      dsum_ += (diff_ * diff_);
    }
  out(0,0) = 20 * log10(nsum_ / dsum_ + 0.000000001); 

}