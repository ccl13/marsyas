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

#include "MarSystem.h"
#include "MarControlManager.h"
#include "EvValUpd.h"
#include "TmVirtualTime.h"

#ifdef MARSYAS_QT
#include "MarGUIManager.h"
#include "MarSystemNetworkGUI.h"
#include "MarSystemControlsGUI.h"
//#include "MarSystemDataGUI.h"
#include "MATLABeditorGUI.h"
#endif

using namespace std;
using namespace Marsyas;

MarSystem::MarSystem(string type, string name)
{
#ifdef MARSYAS_QT
	processMutex_ = new QMutex(QMutex::Recursive);
	msysNetGUI_ = NULL;
	MATLABeditorGUI_ = NULL;
#endif

	parent_ = NULL;
	name_ = name;
	type_ = type;
	active_ = true;
	prefix_ = "/" + type_ + "/" + name_ + "/";
	absPath_ = prefix_;

	isComposite_ = false;
	marsystemsSize_ = 0;

	MATLABscript_ = "";

	isUpdating_ = false;

	//add default controls that
	//all MarSystems should have
	addControls();

	scheduler_.removeAll();
	TmTimer* t = new TmVirtualTime("Virtual",this);
	scheduler_.addTimer(t);
}

// copy constructor
MarSystem::MarSystem(const MarSystem& a)
{
#ifdef MARSYAS_QT
	processMutex_ = new QMutex(QMutex::Recursive);
	msysNetGUI_ = NULL;
	MATLABeditorGUI_ = NULL;
#endif

	parent_ = NULL;
	type_ = a.type_;
	name_ = a.name_;
	prefix_ = a.prefix_;
	absPath_ = a.absPath_;
	active_ = true;


	MATLABscript_ = a.MATLABscript_;

	isUpdating_ = false;

	//clone controls (cloned controls will have no links! - they have to be relinked as done below)
	{
#ifdef MARSYAS_QT
		QWriteLocker locker_w(&rwLock_);
		QReadLocker locker_r(&(a.rwLock_));
#endif

		controls_.clear();
		for (ctrlIter_ = a.controls_.begin(); ctrlIter_ != a.controls_.end(); ++ctrlIter_)
		{
			//clone all controls
			controls_[ctrlIter_->first] = ctrlIter_->second->clone();
			//set new MarSystem parent
			controls_[ctrlIter_->first]->setMarSystem(this);
		}
	}

	//update the "references" to the controls
	ctrl_inSamples_ = getctrl("mrs_natural/inSamples");
	ctrl_inObservations_ = getctrl("mrs_natural/inObservations");
	ctrl_israte_ = getctrl("mrs_real/israte");
	ctrl_inObsNames_ = getctrl("mrs_string/inObsNames");
	ctrl_onSamples_ = getctrl("mrs_natural/onSamples");
	ctrl_onObservations_ = getctrl("mrs_natural/onObservations");
	ctrl_osrate_ = getctrl("mrs_real/osrate");
	ctrl_onObsNames_ = getctrl("mrs_string/onObsNames");
	ctrl_debug_ = getctrl("mrs_bool/debug");
	ctrl_mute_ = getctrl("mrs_bool/mute");
	ctrl_active_ = getctrl("mrs_bool/active");
	ctrl_processedData_ = getctrl("mrs_realvec/processedData");

	//clone children (if any) => mutexes [?]
	isComposite_ = a.isComposite_;
	marsystemsSize_ = 0;
	if (isComposite_)
	{
		for (mrs_natural i=0; i< a.marsystemsSize_; i++)
		{
			MarSystem* clonedChild = (*a.marsystems_[i]).clone();
			addMarSystem(clonedChild);
			clonedChild->relinkControls((*a.marsystems_[i]));
		}
	}

	this->relinkControls(a);

// 	// "re-link" controls  => mutexes [?]
// 	for(ctrlIter_ = a.controls_.begin(); ctrlIter_ != a.controls_.end(); ++ctrlIter_)
// 	{
// 		// get original links...
// 		vector<pair<MarControlPtr, MarControlPtr> > originalLinks = ctrlIter_->second->getLinks();
//
// 		// ...clear clone's links...
// 		//controls_[ctrlIter_->first]->unlinkFromAll(); //[?] is this really necessary?
//
// 		//... and re-establish links between the new cloned controls
// 		vector<pair<MarControlPtr, MarControlPtr> >::const_iterator linksIter;
// 		for (linksIter = originalLinks.begin(); linksIter != originalLinks.end(); ++linksIter)
// 		{
// 			//ignore the root link (not important for relinking)
// 			if(linksIter->first() == linksIter->second())
// 				continue;
//
// 			//check if this control links to someone, and link them accordingly...
// 			if(linksIter->first() == ctrlIter_->second())
// 			{
// 				MarControlPtr ctrl2Link2 = this->getControl(linksIter->second->getMarSystem()->getAbsPath() + linksIter->second->getName(), true);
// 				//controls from siblings may not exist yet at this time, so we must not try to link
// 				//to their yet invalid controls. Just link with controls from
// 				//the parent or already existing siblings and children. The remaining ones will be linked
// 				//by the siblings when they get created.
// 				if (!ctrl2Link2.isInvalid())
// 				{
// 					controls_[ctrlIter_->first]->linkTo(ctrl2Link2);
// 				}
// 			}
// 			//...or check if someone links to this control, and link them accordingly
// 			else if(linksIter->second() == ctrlIter_->second())
// 			{
// 				MarControlPtr linkedCtrl = this->getControl(linksIter->first->getMarSystem()->getAbsPath() + linksIter->first->getName(), true);
// 				//controls from siblings may not exist yet at this time, so we must not try to link
// 				//to their yet invalid controls. Just link with controls from
// 				//the parent or already existing siblings and children. The remaining ones will be linked
// 				//by the siblings when they get created.
// 				if (!linkedCtrl.isInvalid())
// 				{
// 					linkedCtrl->linkTo(controls_[ctrlIter_->first]);
// 				}
// 			}
// 		}
// 	}

	//recreate schedule objects  => mutexes [?]

	scheduler_.removeAll();
	TmTimer* t = new TmVirtualTime("Virtual",this);
	scheduler_.addTimer(t);
}

void
MarSystem::relinkControls(const MarSystem& a)
{
	// "re-link" controls  => mutexes [?]
	for (ctrlIter_ = a.controls_.begin(); ctrlIter_ != a.controls_.end(); ++ctrlIter_)
	{
		// get original links...
		vector<pair<MarControlPtr, MarControlPtr> > originalLinks = ctrlIter_->second->getLinks();

		// ...clear clone's links...
		//controls_[ctrlIter_->first]->unlinkFromAll(); //[?] is this really necessary?

		//... and re-establish links between the new cloned controls
		vector<pair<MarControlPtr, MarControlPtr> >::const_iterator linksIter;
		for (linksIter = originalLinks.begin(); linksIter != originalLinks.end(); ++linksIter)
		{
			//ignore the root link (not important for relinking)
			if (linksIter->first() == linksIter->second())
				continue;

			//check if this control links to someone, and link them accordingly...
			if (linksIter->first() == ctrlIter_->second())
			{
				MarControlPtr ctrl2Link2 = this->getControl(linksIter->second->getMarSystem()->getAbsPath() + linksIter->second->getName(), true);
				//controls from siblings may not exist yet at this time, so we must not try to link
				//to their yet invalid controls. Just link with controls from
				//the parent or already existing siblings and children. The remaining ones will be linked
				//by the siblings when they get created.
				if (!ctrl2Link2.isInvalid())
				{
					controls_[ctrlIter_->first]->linkTo(ctrl2Link2);
				}
			}
			//...or check if someone links to this control, and link them accordingly
			else if (linksIter->second() == ctrlIter_->second())
			{
				MarControlPtr linkedCtrl = this->getControl(linksIter->first->getMarSystem()->getAbsPath() + linksIter->first->getName(), true);
				//controls from siblings may not exist yet at this time, so we must not try to link
				//to their yet invalid controls. Just link with controls from
				//the parent or already existing siblings and children. The remaining ones will be linked
				//by the siblings when they get created.
				if (!linkedCtrl.isInvalid())
				{
					linkedCtrl->linkTo(controls_[ctrlIter_->first]);
				}
			}
		}
	}
}

MarSystem::~MarSystem()
{
	//delete children (if any)
	for (mrs_natural i=0; i< marsystemsSize_; i++)
	{
		delete marsystems_[i];
	}

#ifdef MARSYAS_QT
	delete processMutex_;
	delete msysNetGUI_;
	delete MATLABeditorGUI_;

	//this closes all opened GUIs when the MarSystem is destroyed
	//and clears the internal hash table used to store its pointers
	QList<MarSystemControlsGUI*> controlsGUIs = activeControlsGUIs_.values();
	for (int i = 0; i < controlsGUIs.size(); ++i)
	{
		delete controlsGUIs[i];
	}
	QList<QWidget*> dataGUIs = activeDataGUIs_.values();
	for (int i = 0; i < dataGUIs.size(); ++i)
	{
		delete dataGUIs[i];
	}
#endif
}

void
MarSystem::addControls()
{
	//input pin controls (with state)
	addctrl("mrs_natural/inSamples", MRS_DEFAULT_SLICE_NSAMPLES, ctrl_inSamples_);
	setctrlState(ctrl_inSamples_, true);
	addctrl("mrs_natural/inObservations", MRS_DEFAULT_SLICE_NOBSERVATIONS, ctrl_inObservations_);
	setctrlState(ctrl_inObservations_, true);
	addctrl("mrs_real/israte", MRS_DEFAULT_SLICE_SRATE, ctrl_israte_);
	setctrlState(ctrl_israte_, true);
	addctrl("mrs_string/inObsNames", ",", ctrl_inObsNames_);
	setctrlState(ctrl_inObsNames_, true);

	//output pin controls (stateless)
	addctrl("mrs_natural/onSamples", MRS_DEFAULT_SLICE_NSAMPLES, ctrl_onSamples_);
	addctrl("mrs_natural/onObservations", MRS_DEFAULT_SLICE_NOBSERVATIONS, ctrl_onObservations_);
	addctrl("mrs_real/osrate", MRS_DEFAULT_SLICE_SRATE, ctrl_osrate_);
	addctrl("mrs_string/onObsNames", ",", ctrl_onObsNames_);

	inObservations_ = ctrl_inObservations_->to<mrs_natural>();
	inSamples_ = ctrl_inSamples_->to<mrs_natural>();
	onObservations_ = ctrl_onObservations_->to<mrs_natural>();
	onSamples_ = ctrl_onSamples_->to<mrs_natural>();

	//other controls:
	addctrl("mrs_bool/debug", false, ctrl_debug_);		//no debug by default
	addctrl("mrs_bool/mute", false, ctrl_mute_);			//unmuted by default
	addctrl("mrs_bool/active",true, ctrl_active_);		//active by default

	inTick_.create(inObservations_, inSamples_);
	outTick_.create(onObservations_, onSamples_);

	addctrl("mrs_realvec/processedData", outTick_, ctrl_processedData_);

	ctrl_active_->setState(true);

	active_ = ctrl_active_->to<bool>();
}

bool
MarSystem::addMarSystem(MarSystem *marsystem)
{
#ifdef MARSYAS_QT
	rwLock_.lockForRead();
#endif

	//idiot proof 1
	if (this == marsystem)
	{
		MRSWARN("MarSystem::addMarSystem - Trying to add MarSystem to itself - failing...");
#ifdef MARSYAS_QT
		rwLock_.unlock();
#endif
		return false;
	}

	if (marsystem == NULL)
	{
		MRSWARN("MarSystem::addMarSystem - Adding a NULL MarSystem - failing...");
		return false;
	}

	//idiot proof 2
	MarSystem* msys = parent_;
	while (msys)
	{
		if (msys == marsystem)
		{
			MRSWARN("MarSystem::addMarSystem - Trying to add an ancestor MarSystem as a child - failing...");
#ifdef MARSYAS_QT
			rwLock_.unlock();
#endif
			return false;
		}
		msys = msys->parent_;
	}

	//it's only possible to add MarSystems to Composites
	if (isComposite_)
	{
#ifdef MARSYAS_QT
		rwLock_.unlock();
		rwLock_.lockForWrite();
#endif
		vector<MarSystem*>::iterator it;
		bool replaced = false;
		//check if a child MarSystem with the same type/name
		//exists. If it does, replace it with the new one.
		for (it = marsystems_.begin(); it != marsystems_.end(); ++it)
		{
			if ((*it)->getName() == marsystem->getName() &&
			        (*it)->getType() == marsystem->getType())
			{
				//delete current child MarSystem
				delete (*it);
				//and replace it with the new one
				(*it) = marsystem;
				replaced = true;
				break;
			}
		}
		//if no replacement took place, then add the
		//new MarSystem as a new child
		if (!replaced)
		{
			marsystems_.push_back(marsystem);
			marsystemsSize_ = (mrs_natural)marsystems_.size();
		}
#ifdef MARSYAS_QT
		rwLock_.unlock();
#endif
		//set parent for the new child MarSystem
		marsystem->setParent(this);

		//update child MarSystem
		//marsystem->update(); //superfluous update call! It will be called by this->update() [!]
		//update parent MarSystem
		update();
		return true;
	}
	else
	{
		MRSWARN("MarSystem::addMarSystem - Trying to add MarSystem to a non-Composite - failing...");
#ifdef MARSYAS_QT
		rwLock_.unlock();
#endif
		return false;
	}
}

MarSystem*
MarSystem::getChildMarSystem(std::string childPath)
{
	//check for an absolute path, and if necessary convert it
	//to a relative path
	if (childPath[0] == '/')
	{
		//is this absolute path pointing to this MarSystem?  => mutexes [?]
		if (childPath.substr(0, absPath_.length()) == absPath_)
		{
			//return control path without the absolute path
			//(i.e. return the relative path)
			childPath = childPath.substr(absPath_.length(), childPath.length());
		}
		else
		{
			//this absolute path does not point to this MarSystem or any of its
			//children...
			MRSWARN("MarSystem::getChildMarSystem: " + childPath + " is an invalid path @ " + absPath_);
			return NULL;
		}
	}
	//from this point, childPath is for sure a relative path.

	//start by checking if childPath is not empty (or resulted from an absolute
	//path pointing to this MarSystem itself and not to any of its children)
	if (childPath == "")
	{
		MRSWARN("MarSystem::getChildMarSystem: path does not point to a child MarSystem");
		return NULL;
	}
	//...otherwise, search among its children... => mutexes [?]
	else if (isComposite_)
	{
		vector<MarSystem*>::const_iterator msysIter;
		for (msysIter = marsystems_.begin(); msysIter != marsystems_.end(); ++msysIter)
		{
			string prefix = (*msysIter)->getPrefix();
			prefix = prefix.substr(1, prefix.length()-2); //ignore leading and trailing "/"
			if (childPath.substr(0, prefix.length()) == prefix)
			{
				//a matching child was found!
				if (childPath.length() == prefix.length())
					return (*msysIter);
				//The prefix may be the same, but we still need to check that the next char is a /
				// Otherwise in the case of system Sys1, and Sys10, it will appear that Sys10 is the child Sys1
				else if (childPath.length() > prefix.length() && childPath.substr(prefix.length() , 1) == "/")
				{
					//remove parent prefix from childPath, and continue searching recursively in children
					childPath = childPath.substr(prefix.length()+1,childPath.length());
					return (*msysIter)->getChildMarSystem(childPath);
				}
			}
		}
		MRSWARN("MarSystem::getChildMarsystem(): " + childPath + " not found!");
		return NULL;
	}
	else
	{
		MRSWARN("MarSystem::getChildMarsystem(): " + childPath + " not found!");
		return NULL;
	}
}

void
MarSystem::setParent(const MarSystem* parent) // => mutexes [?]
{
	parent_ = const_cast<MarSystem*>(parent);

	// Update our path, and propogate changes to children (if applicable)
	updatePath();
}

void
MarSystem::setName(string name) // => mutexes [?]
{
	if (name == name_)
		return;

	string oldPrefix = prefix_;
	prefix_ = "/" + type_ + "/" + name + "/";
	name_ = name;

	//update path accordingly
	string::size_type pos = absPath_.find(oldPrefix, 0);
	string uppath = absPath_.substr(0, pos);
	string downpath = absPath_.substr(oldPrefix.length()+pos, absPath_.length()-(oldPrefix.length()+pos));
	absPath_ = uppath + prefix_ + downpath;

	if (isComposite_)
	{
		for (int i=0; i<marsystemsSize_; i++)
		{
			marsystems_[i]->updatePath();
		}
	}
}

void
MarSystem::setType(string type) // => mutexes [?]
{
	if (type == type_)
		return;

	string oldPrefix = prefix_;
	prefix_ = "/" + type + "/" + name_ + "/";
	type_ = type;

	//update path accordingly
	string::size_type pos = absPath_.find_last_of(oldPrefix, 0);
	string uppath = absPath_.substr(0, pos);
	string downpath = absPath_.substr(oldPrefix.length()+pos, absPath_.length()-(oldPrefix.length()+pos));
	absPath_ = uppath + prefix_ + downpath;

	if (isComposite_)
	{
		for (int i=0; i<marsystemsSize_; i++)
		{
			marsystems_[i]->updatePath();
		}
	}
}

string
MarSystem::getType() const // => mutexes [?]
{
	return type_;
}

string
MarSystem::getName() const // => mutexes [?]
{
	return name_;
}

string
MarSystem::getPrefix() const // => mutexes [?]
{
	return prefix_;
}

string
MarSystem::getAbsPath() const // => mutexes [?]
{
	return absPath_;
}

void
MarSystem::updatePath() // => mutexes [?]
{
	if (parent_)
	{
		absPath_ = parent_->getAbsPath() + type_ + '/' + name_ + '/';
	}
	else
	{
		absPath_ = prefix_;
	}

	//propagate new path to all children (if any)
	if (isComposite_)
		for (mrs_natural i=0; i< marsystemsSize_; ++i)
			marsystems_[i]->updatePath();
}

void
MarSystem::checkFlow(realvec& in, realvec& out)
{
	irows_ = in.getRows();
	icols_ = in.getCols();
	orows_ = out.getRows();
	ocols_ = out.getCols();

	if (ctrl_debug_->isTrue())
	{
		MRSWARN("Debug CheckFlow Information");
		MRSWARN("MarSystem Type    = " << type_);
		MRSWARN("MarSystem Name    = " << name_);
		MRSWARN("inObservAtions_ = " << inObservations_);
		MRSWARN("inSamples_ = " << inSamples_);
		MRSWARN("onObservations_ = " << onObservations_);
		MRSWARN("onSamples_ = " << onSamples_);
		MRSWARN("Input  Slice Rows = " << irows_ );
		MRSWARN("Input  Slice Cols = " << icols_ );
		MRSWARN("Output Slice Rows = " << orows_ );
		MRSWARN("Output Slice Cols = " << ocols_ );
	}


	MRSASSERT(irows_ == inObservations_);
	MRSASSERT(icols_ == inSamples_);
	MRSASSERT(orows_ == onObservations_);
	MRSASSERT(ocols_ == onSamples_);
}

void
MarSystem::process(realvec& in, realvec& out)
{
#ifdef MARSYAS_QT
	processMutex_->lock();
#endif

	checkFlow(in, out); //shouldn't this be only active when MARSYAS_DEBUG if defined?

	myProcess(in, out);

#ifdef MARSYAS_MATLAB
	if (!MATLABscript_.empty())
	{
		MATLAB_PUT(in, name_ + "_in");
		MATLAB_PUT(out, name_ + "_out");
		MATLAB_EVAL(MATLABscript_);
		MATLAB_GET(name_+"_out", out);

		//check if out realvec was shortened by MATLAB script... //[!]
		if ((out.getRows() < onObservations_)||(out.getCols() < onSamples_))
			out.stretch(onObservations_, onSamples_);
	}
#endif

#ifdef MARSYAS_QT
	processMutex_->unlock();
	//emit processed();
#endif
}

void
MarSystem::tick()
{
	//if MarSystem is not active, ignore ticks
	if (ctrl_active_->isTrue())
	{
		scheduler_.tick();
		{
			MarControlAccessor acc(ctrl_processedData_);
			realvec& processedData = acc.to<mrs_realvec>();
			process(inTick_, processedData);
		}
	}
	else
	{
		MRSDIAG("MarSystem::tick() : MarSystem is not active! Ignoring tick command.");
		(void)42;
	}
}

void
MarSystem::myUpdate(MarControlPtr sender)
{
	(void) sender;
	MRSDIAG("MarSystem.cpp - MarSystem:myUpdate");

	//lmartins:
	//By default, a MarSystem does not modify the input data stream format.
	//Override this method on a derived MarSystem if data format changes
	//should take place...

	//forward flow propagation
	ctrl_onSamples_->setValue(ctrl_inSamples_, NOUPDATE);
	ctrl_onObservations_->setValue(ctrl_inObservations_, NOUPDATE);
	ctrl_osrate_->setValue(ctrl_israte_, NOUPDATE);
	ctrl_onObsNames_->setValue(ctrl_inObsNames_, NOUPDATE);
}

bool
MarSystem::isUpdating()
{
#ifdef MARSYAS_QT
	QReadLocker locker(&processMutex_);
#endif

	return isUpdating_;
}

void
MarSystem::update(MarControlPtr sender)
{
#ifdef MARSYAS_QT
	processMutex_->lock();
#endif

	MRSDIAG("MarSystem.cpp - MarSystem:Update");

	isUpdating_ = true;

	//store current flow variables
	tinObservations_ = inObservations_;
	tinSamples_ = inSamples_;
	tisrate_ = israte_;
	tinObsNames_ = inObsNames_;
	tonObservations_ = onObservations_;
	tonSamples_ = onSamples_;
	tosrate_ = osrate_;
	tonObsNames_ = onObsNames_;

	//sync input member variables
	inObservations_ = ctrl_inObservations_->to<mrs_natural>();
	inSamples_ = ctrl_inSamples_->to<mrs_natural>();
	israte_ = ctrl_israte_->to<mrs_real>();
	inObsNames_ = ctrl_inObsNames_->to<mrs_string>();
	//sync output member variables
	onObservations_ = ctrl_onObservations_->to<mrs_natural>();
	onSamples_ = ctrl_onSamples_->to<mrs_natural>();
	osrate_ = ctrl_osrate_->to<mrs_real>();
	onObsNames_ = ctrl_onObsNames_->to<mrs_string>();

	//call derived class specific update
	myUpdate(sender);

	//sync input member variables
	inObservations_ = ctrl_inObservations_->to<mrs_natural>();
	inSamples_ = ctrl_inSamples_->to<mrs_natural>();
	israte_ = ctrl_israte_->to<mrs_real>();
	inObsNames_ = ctrl_inObsNames_->to<mrs_string>();
	//sync output member variables
	onObservations_ = ctrl_onObservations_->to<mrs_natural>();
	onSamples_ = ctrl_onSamples_->to<mrs_natural>();
	osrate_ = ctrl_osrate_->to<mrs_real>();
	onObsNames_ = ctrl_onObsNames_->to<mrs_string>();

	//check active status
	bool active = ctrl_active_->isTrue();
	//if active status changed...
	if (active_ !=  active)
	{
		active_ = active;
		activate(active);
	}

	//resize input and output realvec if necessary
	if ((inObservations_ != inTick_.getRows()) ||
	        (inSamples_ != inTick_.getCols())      ||
	        (onObservations_ != outTick_.getRows()) ||
	        (onSamples_ != outTick_.getCols()))
	{
		inTick_.create(inObservations_, inSamples_);
		{
			MarControlAccessor acc(ctrl_processedData_);
			realvec& processedData = acc.to<mrs_realvec>();
			processedData.create(onObservations_, onSamples_);
		}
	}

	//check for OUT-FLOW modifications without parent knowledge!!
	if (parent_)
	{
		if (tonObservations_ != onObservations_ ||
		        tonSamples_ != onSamples_ ||
		        tosrate_ != osrate_ ||
		        tonObsNames_ != onObsNames_)
			if (!parent_->isUpdating())
				parent_->update(sender);
	}

	isUpdating_ = false;

#ifdef MARSYAS_QT
	processMutex_->unlock();
#endif
}

void
MarSystem::activate(bool state) //non-thread-safe, but this method is only supposed to be called from update(), which is thread-safe
{
	//since this method must be public (so it can be called in Composite::activate())
	//we must guarantee that the "mrs_bool/active" control is in sync with any eventual
	//direct calls to MarSystem::activate() from client code
	if (ctrl_active_->to<bool>() != state)
	{
		ctrl_active_->setValue(state, NOUPDATE);
		active_ = state;
	}

	//execute any code needed to run when activating/deactivating
	//the derived MarSystem
	localActivate(state);
}

void
MarSystem::localActivate(bool state)
{
	//call activate for all Composite's components
	if (isComposite_)
	{
		for (mrs_natural i=0; i< marsystemsSize_; i++)
		{
			//marsystems_[i]->activate(state);
			marsystems_[i]->updctrl("mrs_bool/active", state); //thread-safe
		}
	}
}

string
MarSystem::getControlRelativePath(string cname) const
{
	// If the cname starts with an '/', it is an absolute path
	// that has to be made relative.
	if (cname[0] == '/')
	{
		//is this absolute path pointing to this MarSystem? // => mutexes [?]
		if (cname.substr(0, absPath_.length()) == absPath_)
		{
			//return control path without the absolute path
			//(i.e. return the relative path)
			return cname.substr(absPath_.length(), cname.length());
		}
		else
		{
			return "";
		}
	}
	else
	{
		// cname is already a relative path, so we can return that.
		return cname;
	}
}

string
MarSystem::getControlLocalPath(string cname) const
{
	// Convert cname to the canonical relative format.
	cname = getControlRelativePath(cname);
	if (cname == "")
	{
		// Not a control from this MarSystem or its children.
		return "";
	}

	// A local path should have only one '/' (e.g. "mrs_xxx/nnnn"),
	// otherwise it's probably a control from a child MarSystem.
	if (cname.find_first_of('/') == cname.find_last_of('/') &&
	        cname.find_first_of('/') != string::npos)
	{
		// This is a relative and local path, so just return it
		return cname;
	}
	else
	{
		// This is not a local control => return invalid name
		return "";
	}
}

bool
MarSystem::linkControl(string cname1, string cname2, bool update)
{
	//Links the control cname1 with the control cname 2.
	//
	//Link fails case both cname1 and cname2 point to non-existing controls.
	//
	//cname1 should point to a local or child control (using an absolute control pathname,
	//a local control pathname or a relative pathname).
	//If control corresponding to cname1 does not exist locally or in any child,
	//a proxy control is created and added to the corresponding MarSystem.
	//In all these cases, cname1 control will assume the current value of the cname2 control.
	//
	//cname2 should point to an existing control, anywhere in the network (i.e. not necessarily a
	//local or a child's control).
	//cname 2 can be a absolute, local or relative pathname.
	//In case cname2 points to a non-existing control, if its pathname corresponds to a local or
	//a child's control, a proxy control will be created in the corresponding MarSystem.
	//In this case, the value of the cname2 control will be set to the current value of the cname1 control.

	//try to get the controls
	MarControlPtr ctrl1 = getControl(cname1, false, true);//search local and child controls
	MarControlPtr ctrl2 = getControl(cname2, true, true);//search everywhere in the network (including locally, at parent and among children)

	//a control is inherently connected to itself!
	if (ctrl1() == ctrl2())
	{
		if (ctrl1() == NULL)
		{
			MRSWARN("MarSystem::linkControl - Impossible to link two non-existing controls: " + cname1+" --> "+cname2);
			return false;
		}
		else
			return true;
	}

	//if  2nd control does not exist somewhere in the network,
	//try to create one locally or in children (and assign it the value of the 1st control)
	if (ctrl2.isInvalid())
	{
		//MRSWARN("MarSystem::linkControl - 2nd control does not exist anywhere: " + cname2 + " -> THIS MAY BE NORMAL WHEN LOADING A MARSYSTEM NETWORK FROM A .mpl FILE!");
		//return false;

		string relativecname = getControlRelativePath(cname2);
		string localcname = getControlLocalPath(cname2);

		//if cname2 is a local control path, add it to this MarSystem
		if (localcname != "")
		{
			if (!addControl(cname2, ctrl1->clone(), ctrl2))
			{
				MRSWARN("MarSystem::linkControl - Error creating new proxy control " + cname2 + " @ " + getAbsPath());
				return false;
			}
			MRSDIAG("MarSystem::linkControl - Added new proxy control " + cname2 + " @ " + getAbsPath());
			ctrl2->setState(false); //proxy controls never need to have state!
		}
		//if cname2 is a relative path, check among children for a matching MarSystem
		//where to add the new link control
		else if (relativecname != "")
		{
			//get the MarSystem path from relativecname
			string::size_type pos = relativecname.find("/mrs_", 0);
			string relativepath = relativecname.substr(0, pos);

			MarSystem* msys = getChildMarSystem(relativepath);
			if (msys)
			{
				string cname = relativecname.substr(pos+1, relativecname.length());
				if (!msys->addControl(cname, ctrl1->clone(), ctrl2))
				{
					MRSWARN("MarSystem::linkControl - Error creating new link control " + cname2 + " @ " + msys->getAbsPath());
					return false;
				}
				MRSDIAG("MarSystem::linkControl - Added new proxy control " + cname2 + " @ " + msys->getAbsPath());
				ctrl2->setState(false); //proxy controls never need to have state!
			}
			else
			{
				MRSWARN("MarSystem::linkControl - Error creating new link control: " + cname2 + " is an invalid path");
				return false;
			}
		}
		//if cname1 path is not a valid path, nor a path pointing to any of the children
		//it is not possible to add the new link control...
		else
		{
			MRSWARN("MarSystem::linkControl - Error creating new link control: " + cname2 + " is an invalid path");
			return false;
		}
	}

	//check if the first control already exists or if we have
	// to create and add it to the corresponding MarSystem
	if (ctrl1.isInvalid())
	{
		string relativecname = getControlRelativePath(cname1);
		string localcname = getControlLocalPath(cname1);

		//if cname1 is a local control path, add it to this MarSystem
		if (localcname != "")
		{
			if (!addControl(cname1, ctrl2->clone(), ctrl1))
			{
				MRSWARN("MarSystem::linkControl - Error creating new link control " + cname1 + " @ " + getAbsPath());
				return false;
			}
			MRSDIAG("MarSystem::linkControl - Added new proxy control " + cname1 + " @ " + getAbsPath());
			ctrl1->setState(false); //proxy controls never need to have state!
		}
		//if cname1 is a relative path, check among children for a matching MarSystem
		//where to add the new link control
		else if (relativecname != "")
		{
			//get the MarSystem path from relativecname
			string::size_type pos = relativecname.find("/mrs_", 0);
			string relativepath = relativecname.substr(0, pos);

			MarSystem* msys = getChildMarSystem(relativepath);
			if (msys)
			{
				string cname = relativecname.substr(pos+1, relativecname.length());
				if (!msys->addControl(cname, ctrl2->clone(), ctrl1))
				{
					MRSWARN("MarSystem::linkControl - Error creating new link control " + cname1 + " @ " + msys->getAbsPath());
					return false;
				}
				MRSDIAG("MarSystem::linkControl - Added new proxy control " + cname1 + " @ " + msys->getAbsPath());
				ctrl1->setState(false); //proxy controls never need to have state!
			}
			else
			{
				MRSWARN("MarSystem::linkControl - Error creating new link control: " + cname1 + "is an invalid path");
				return false;
			}
		}
		//if cname1 path is not a valid path, nor a path pointing to any of the children
		//it is not possible to add the new link control...
		else
		{
			MRSWARN("MarSystem::linkControl - Error creating new link control: " + cname1 + "is an invalid path");
			return false;
		}
	}

	//now both controls exist
	//just link them
	return ctrl1->linkTo(ctrl2, update);
}

MarControlPtr
MarSystem::getControl(string cname, bool searchParent, bool searchChildren)
{
	//USE A CACHE FOR MORE EFFICIENT LOOK-UP?? [!]

	//convert cname to the canonical relative format
	string relativecname = getControlRelativePath(cname);

	//if this is not a control from this MarSystem or its children
	//ask parent (if allowed) to search for it on the remaining of the network
	if (relativecname == "")
	{
		if (searchParent && parent_)
		{
			return parent_->getControl(cname, true, true);//parent will also ask its parent if necessary // => mutexes [?]
		}
		else //parent search not allowed, so the control was not found => return invalid control
		{
			//MRSWARN("MarSystem::getControl - Unsupported control name: " + cname + " @ " + absPath_ + " -> THIS MAY BE NORMAL DURING CLONING!");
			return MarControlPtr();
		}
	}
	//check if this relative control path points to a possible local control
	string localcname = getControlLocalPath(relativecname);
	if (localcname != "")
	{
		//This may be a local control, so look for it
#ifdef MARSYAS_QT
		QReadLocker locker(&rwLock_); //reading controls_ [!]
#endif
		if (controls_.find(localcname) != controls_.end())
		{
			return controls_[localcname]; //control found
		}
		else
		{
			return MarControlPtr(); //no control found with this name => return invalid control
		}
	}
	//definitely not a local control pathname. It can only be a relative control path.
	//So search in children (if allowed).
	else
	{
		if (searchChildren)
		{
			//search for a child that has a corresponding prefix
			vector<MarSystem*>::const_iterator msysIter;
			for (msysIter = marsystems_.begin(); msysIter != marsystems_.end(); ++msysIter) // => mutexes [?]
			{
				string prefix = (*msysIter)->getPrefix();
				prefix = prefix.substr(1, prefix.length()); //ignore leading "/"
				if (relativecname.substr(0, prefix.length()) == prefix)
				{
					//a matching child was found!
					//check if the control exists in the child
					string childcname = relativecname.substr(prefix.length(),relativecname.length());
					return (*msysIter)->getControl(childcname);
				}
			}
			return MarControlPtr();//no child found with corresponding prefix...
		}
		else
		{
			return MarControlPtr();
		}
	}
}

bool
MarSystem::hasControlState(string cname)
{
	MarControlPtr control = getControl(cname);
	if (control.isInvalid())
	{
		MRSWARN("MarSystem::hasControlState Unsupported control name = " + cname);
		return false;
	}
	else
		return control->hasState();
}

void
MarSystem::setControlState(string cname, bool state)
{
	MarControlPtr control = getControl(cname);
	if (control.isInvalid())
	{
		MRSWARN("MarControls::setState Unsupported control name = " + cname);
	}
	else
		control->setState(state);
}

bool
MarSystem::hasControl(string cname, bool searchChildren)
{
	//look for local and (optionally) children controls
	MarControlPtr control = this->getControl(cname, false, searchChildren);
	return !control.isInvalid();
}

bool
MarSystem::hasControl(MarControlPtr control, bool searchChildren)
{
#ifdef MARSYAS_QT
	QReadLocker locker(&rwLock_);
#endif

	//search local controls
	for (ctrlIter_=controls_.begin(); ctrlIter_!=controls_.end();++ctrlIter_)
	{
		if ((ctrlIter_->second)() == control())
			return true;
	}
	//search control among children (if allowed)
	if (searchChildren)
	{
		vector<MarSystem*>::const_iterator msysIter;
		for (msysIter=marsystems_.begin(); msysIter!=marsystems_.end(); ++msysIter)
		{
			if ((*msysIter)->hasControl(control, true))
				return true;
		}
	}
	return false;
}

bool
MarSystem::updControl(MarControlPtr control, MarControlPtr newcontrol, bool upd)
{
	// check if the control is valid
	if (control.isInvalid())
	{
		MRSWARN("MarSystem::updControl - Invalid control ptr @ " + getAbsPath());
		return false;
	}

	if (newcontrol.isInvalid())
	{
		MRSWARN("MarSystem::updControl - Invalid control ptr given for assignment");
		return false;
	}
	// 	//check if control exists locally or among children
	// 	if(!hasControl(control))
	// 	{
	// 		MRSWARN("MarSystem::updControl - " + control->getName() +" @ "+getAbsPath()+ " does not exist locally or in children!");
	// 		return false;
	// 	}

	return control->setValue(newcontrol, upd);
}

//get local controls only (i.e. no child controls included)
const map<string, MarControlPtr>&
MarSystem::getLocalControls()
{
#ifdef MARSYAS_QT
	QReadLocker locker(&rwLock_); //reading controls_ [!]
#endif

	return controls_;
}

// get all the controls (including controls of children)
// for a particular MarSystem
map<string, MarControlPtr>
MarSystem::getControls(map<string, MarControlPtr>* cmap)
{
#ifdef MARSYAS_QT
	QReadLocker locker(&rwLock_);
#endif

	if (!cmap)
	{
		map<string, MarControlPtr> controlsmap;

		cmap = &controlsmap;

		//fill list with local controls
		for (ctrlIter_=controls_.begin(); ctrlIter_ != controls_.end(); ++ctrlIter_)
		{
			(*cmap)[absPath_+ctrlIter_->first] = ctrlIter_->second;
		}

		//iterate over children, recursively,
		//and fill the list with their controls
		vector<MarSystem*>::const_iterator msysIter;
		for (msysIter = marsystems_.begin(); msysIter != marsystems_.end(); ++msysIter)
		{
			(*msysIter)->getControls(cmap);
		}

		return (*cmap);
	}
	else
	{
		//fill list with local controls
		for (ctrlIter_=controls_.begin(); ctrlIter_ != controls_.end(); ++ctrlIter_)
		{
			(*cmap)[absPath_+ctrlIter_->first] = ctrlIter_->second;
		}

		//iterate over children, recursively,
		//and fill the list with their controls
		vector<MarSystem*>::const_iterator msysIter;
		for (msysIter = marsystems_.begin(); msysIter != marsystems_.end(); ++msysIter)
		{
			(*msysIter)->getControls(cmap);
		}

		return (*cmap);
	}
}

vector<MarSystem*>
MarSystem::getChildren()
{
#ifdef MARSYAS_QT
	QReadLocker locker(&rwLock_);
#endif
	return marsystems_;
}

bool
MarSystem::addControl(string cname, MarControlPtr v, MarControlPtr& ptr)
{
	if (addControl(cname, v))
	{
		ptr = controls_[cname]; // => mutexes [?]
		return true;
	}
	else
	{
		ptr = MarControlPtr();//return invalid control
		return false;
	}
}

bool
MarSystem::addControl(string cname, MarControlPtr v)
{
	//convert cname to the canonical local control pathname format
	string pcname = cname;
	cname = getControlLocalPath(cname);
	if (cname == "")
	{
		//cname is an invalid control pathname!
		MRSWARN("MarSystem::addControl - invalid control pathname: " + pcname);
		MRSWARN("MarSystem::addControl - absolute path: " + absPath_); // => mutexes [?]
		return false;
	}

	//check for type mismatch between cname string (which include type information)
	//and the actual control type passed as an argument
	string::size_type pos = cname.find("/", 0);
	string ctype = cname.substr(0,pos);
	if (ctype!= v->getType())
	{
		MRSWARN("MarSystem::addControl control type mismatch (" + ctype + "!=" + v->getType() + ")");
		return false;
	}

#ifdef MARSYAS_QT
	QWriteLocker locker(&rwLock_); //writing controls_
#endif
	controls_[cname] = v;
	controls_[cname]->setMarSystem(this);
	controls_[cname]->setName(cname);

	//success!
	MRSDIAG("MarSystem::addControl - control added successfully: " + cname + " @ " + absPath_);
	return true;
}

void
MarSystem::updctrl(EvEvent* me)
{
	if (me != NULL)
	{
		me->dispatch();
		delete(me);
	}
}

void
MarSystem::updctrl(TmTime t, EvEvent* ev)
{
	scheduler_.post(t,Repeat(),ev);
}

void
MarSystem::updctrl(TmTime t, Repeat r, EvEvent* ev)
{
	scheduler_.post(t,r,ev);
}

void
MarSystem::updctrl(TmTime t, string cname, MarControlPtr control)
{
	scheduler_.post(t,Repeat(),new EvValUpd(this,cname,control));
}

void
MarSystem::updctrl(TmTime t, Repeat r, string cname, MarControlPtr control)
{
	scheduler_.post(t,r,new EvValUpd(this,cname,control));
}

void
MarSystem::removeTimer(string name)
{
	scheduler_.removeTimer(name);
}

void
MarSystem::addTimer(std::string tmr_class, std::string tmr_ident)
{
	scheduler_.addTimer(tmr_class,tmr_ident);
}
void
MarSystem::addTimer(std::string tmr_class, std::string tmr_ident, std::vector<TmParam> params)
{
	scheduler_.addTimer(tmr_class,tmr_ident,params);
}

void
MarSystem::updtimer(std::string tmr_ctrl_path, TmControlValue value)
{
	scheduler_.updtimer(tmr_ctrl_path,value);
}
void
MarSystem::updtimer(std::string tmr_path, TmParam param)
{
	scheduler_.updtimer(tmr_path,param);
}
void
MarSystem::updtimer(std::string tmr_path, std::vector<TmParam> params)
{
	scheduler_.updtimer(tmr_path,params);
}



mrs_natural
MarSystem::getTime(string timer_name)
{
	return scheduler_.getTime(timer_name);
}

void
MarSystem::setMATLABscript(std::string script)
{
#ifdef MARSYAS_QT
	processMutex_->lock();
#endif

	MATLABscript_ = script;

#ifdef MARSYAS_QT
	processMutex_->unlock();
#endif
}

string
MarSystem::getMATLABscript()
{
#ifdef MARSYAS_QT
	processMutex_->lock();
#endif

	return MATLABscript_;

#ifdef MARSYAS_QT
	processMutex_->unlock();
#endif
}

mrs_real*
const MarSystem::recvControls()
{
	return NULL;
}

string
MarSystem::toString()
{
	ostringstream oss;
	put(oss);
	return oss.str();
}

marostring&
MarSystem::toString(marostring& m)
{
	m.begin_marsystem(isComposite_, getType(), getName());

	m.begin_controls(controls_.size());
	for (ctrlIter_ = controls_.begin(); ctrlIter_ != controls_.end(); ++ctrlIter_)
	{
		MarControlPtr c = ctrlIter_->second;
		std::ostringstream cv;
		cv << c;
		std::string ct = c->getType();
		std::string cn = c->getName();
		// get the trailing bit after / Don't know why name is now type/name
		int lp = (int)cn.find_last_of('/');
		if (lp >= 0 && (lp+1) < (int)cn.size())
			cn = cn.substr(lp+1);

		bool cs = c->hasState();
		m.begin_control(ct, cn, cv.str(), cs);

		std::vector<std::pair<MarControlPtr, MarControlPtr> > links = c->getLinks();
		int j=0;
		for (size_t i=0; i<links.size(); i++)
		{
			//check who is linking to this control, but avoid outputting root link info
			if (c == links[i].second() && links[i].first() != c)
			{
				j++;
			}
		}
		m.begin_control_links_in(j);
		for (size_t i=0; i<links.size(); i++)
		{
			// check who is linking to this control, but avoid outputting root link info
			if (c == links[i].second() && links[i].first() != c)
			{
				m.put_control_link_in(links[i].first->getMarSystem()->getAbsPath(),links[i].first->getType(),links[i].first->getName());
			}
		}
		m.end_control_links_in(j);

		for (size_t i=0; i<links.size(); i++)
		{
			//check where to this control is linking, but avoid outputting root link info
			if (c == links[i].first() && c != links[i].second())
			{
				j++;
			}
		}
		m.begin_control_links_out(j);
		for (size_t i=0; i<links.size(); i++)
		{
			//check where to this control is linking, but avoid outputting root link info
			if (c == links[i].first() && c != links[i].second())
			{
				m.put_control_link_out(links[i].second->getMarSystem()->getAbsPath(),links[i].second->getType(),links[i].second->getName());
			}
		}
		m.end_control_links_out(j);
		m.end_control(ct, cn, cv.str(), cs);
	}
	m.end_controls(controls_.size());

	int sz=marsystems_.size();
	if (sz>0)
	{
		m.begin_children(sz);
		for (int i=0; i<sz; i++)
		{
			marsystems_[i]->toString(m);
		}
		m.end_children(sz);
	}
	m.end_marsystem(isComposite_, getType(), getName());
	return m;
}

// write *this to s
ostream&
MarSystem::put(ostream &o)
{
#ifdef MARSYAS_QT
	QReadLocker locker(&rwLock_);
#endif

	if (isComposite_)
	{
		o << "# MarSystemComposite" << endl;
	}
	else
	{
		o << "# MarSystem" << endl;
	}
	o << "# Type = " << type_ << endl;
	o << "# Name = " << name_ << endl;

	o << endl;
	o << "# MarControls = " << controls_.size() << endl;
	for (ctrlIter_=controls_.begin(); ctrlIter_ != controls_.end(); ++ctrlIter_)
	{
		ostringstream toss;
		toss << ctrlIter_->second;
		if (toss.str() != "")
			o << "# " << ctrlIter_->first << " = " << ctrlIter_->second << endl;
		else
			o << "# " << ctrlIter_->first << " = " << "MARSYAS_EMPTYSTRING" << endl;
		//serialize links
		ostringstream oss;
		std::vector<std::pair<MarControlPtr, MarControlPtr> > links = ctrlIter_->second->getLinks();
		mrs_natural numLinks = 0;
		//links to:
		for (size_t i=0; i<links.size(); i++)
		{
			//check where to this control is linking, but avoid outputting root link info
			if (ctrlIter_->second() == links[i].first() && links[i].first() != links[i].second())
			{
				oss << "# " << links[i].second->getMarSystem()->getAbsPath() << links[i].second->getName() << endl;
				numLinks++;
			}
		}
		o << "# LinksTo = " << numLinks << endl << oss.str();
		//linked from:
		numLinks = 0;
		oss.str(""); //clear the stringstream
		for (size_t i=0; i<links.size(); i++)
		{
			//check who is linking to this control, but avoid outputting root link info
			if (ctrlIter_->second() == links[i].second() && links[i].first() != links[i].second())
			{
				oss << "# " << links[i].first->getMarSystem()->getAbsPath() << links[i].first->getName() << endl;
				numLinks++;
			}
		}
		o << "# LinkedFrom = " << numLinks << endl << oss.str();
	}

	if (isComposite_)
	{
		o << endl;
		o << "# nComponents = " << marsystemsSize_ << endl;
		o << endl;

		for (mrs_natural i=0; i < marsystemsSize_; i++)
			o << *(marsystems_[i]) << endl;
	}

	return o;
}

//
// Output the MarSystem to stdout as a HTML document, with
// collapsible elements provided by a Javascript function
//
ostream&
MarSystem::put_html(ostream &o)
{
	//
	// Output the HTML header
	//
	// sness - Add proper DOCTYPE later
	o << "<script type=\"text/javascript\" src=\"http://assets.sness.net/simpletreemenu.js\">" << endl;
	o << "</script>" << endl;
	o << "<link rel=\"stylesheet\" type=\"text/css\" href=\"http://assets.sness.net/simpletree.css\" />" << endl;
	o << "<a href=\"javascript:ddtreemenu.flatten(\'treemenu1\', \'expand\')\">Expand All</a>" << endl;
	o << "<a href=\"javascript:ddtreemenu.flatten(\'treemenu1\', \'contact\')\">Contact All</a>" << endl;
	o << "<ul id=\"treemenu1\" class=\"treeview\">" << endl;

	put_html_worker(o);

	//
	// Output the HTML footer
	//
	o << "<script type=\"text/javascript\">" << endl;
	o << "ddtreemenu.createTree(\"treemenu1\", true)" << endl;
	o << "</script>" << endl;

	return o;
}

//
// Output a single MarSystem to &o, without HTML headers and footers
//
ostream&
MarSystem::put_html_worker(ostream &o)
{

	if (isComposite_)
	{
		o << "<li>MarSystemComposite" << endl;
	}
	else
	{
		o << "<li>MarSystem" << endl;
	}
	o << "Type = " << type_ << endl;
	o << "Name = " << name_ << endl;

	o << endl;
	o << "<li>MarControls" << controls_.size() << endl;
	o << "<ul>" << endl;
	for (ctrlIter_=controls_.begin(); ctrlIter_ != controls_.end(); ++ctrlIter_)
	{
		ostringstream toss;
		toss << ctrlIter_->second;
		if (toss.str() != "")
			o << "<li>" << ctrlIter_->first << " = " << ctrlIter_->second << "</li>" << endl;
		else
			o << "<li>" << ctrlIter_->first << " = " << "MARSYAS_EMPTYSTRING" << "</li>" << endl;

		//serialize links
		ostringstream oss;
		std::vector<std::pair<MarControlPtr, MarControlPtr> > links = ctrlIter_->second->getLinks();
		mrs_natural numLinks = 0;

		//
		// Links To:
		//
		for (size_t i=0; i<links.size(); i++)
		{
			//check where to this control is linking, but avoid outputting root link info
			if (ctrlIter_->second() == links[i].first() && links[i].first() != links[i].second())
			{
				oss << "<li>" << links[i].second->getMarSystem()->getAbsPath() << links[i].second->getName() << "</li>" << endl;
				numLinks++;
			}
		}
		if (numLinks > 0)
			o << "<li>LinksTo = " << numLinks << endl << "<ul>" << oss.str() << "</ul></li>";

		//
		// Linked From:
		//
		numLinks = 0;
		oss.str(""); //clear the stringstream
		for (size_t i=0; i<links.size(); i++)
		{
			//check who is linking to this control, but avoid outputting root link info
			if (ctrlIter_->second() == links[i].second() && links[i].first() != links[i].second())
			{
				oss << "<li>" << links[i].first->getMarSystem()->getAbsPath() << links[i].first->getName() << "</li>" << endl;
				numLinks++;
			}
		}
		if (numLinks > 0)
			o << "<li>LinkedFrom = " << numLinks << endl << "<ul>" << oss.str() << "</ul></li>";
	}
	o << "</ul>" << endl;
	o << "</li>" << endl;

	if (isComposite_)
	{
		o << endl;
		o << "<li>Components = " << marsystemsSize_ << endl;
		o << "<ul>" << endl;

		for (mrs_natural i=0; i < marsystemsSize_; i++)
			(marsystems_[i])->put_html_worker(o);

		o << "</ul>" << endl;
		o << "</li>" << endl;
	}

	o << "</li>" << endl;


	return o;
}

istream&
MarSystem::put(istream& is)
{
	/* This method assumes that the input stream is in the format
	 * generated by MarSystem::put(ostream& o).
	 * This is also known as ".mpl file format".
	 *
	 * This method also assumes that it is being called by
	 * MarSystemManager::getMarSystem(std::istream& is, MarSystem *parent)
	 * and thus the input has its header data (first 4 lines) stripped off already
	 */

	string skipstr;
	mrs_natural i;
	string type;
	string rstr = "mrs_real";
	string nstr = "mrs_natural";
	string bstr = "mrs_bool";
	string sstr = "mrs_string";
	string vstr = "mrs_realvec";
	mrs_real   rcvalue;
	string scvalue;
	mrs_natural ncvalue;
	bool bcvalue;
	string cname;
	map<string, MarControlPtr>::iterator iter;

#ifdef MARSYAS_QT
	rwLock_.lockForWrite();
#endif

	// if composite, clear all children to avoid bad links to prototype children
	if (isComposite_)
	{
		for (mrs_natural i=0; i< marsystemsSize_; i++)
		{
			delete marsystems_[i];
		}
		marsystems_.clear();
		marsystemsSize_ = 0;
	}

#ifdef MARSYAS_QT
	rwLock_.unlock();
	QReadLocker lock(&(rwLock_));
#endif

	// Start reading!

	/* The first line should look something like:
	 * # MarControls = 19
	 * we want to skip everything up to the 19 (or whatever number it is)
	 */
	is >> skipstr >> skipstr >> skipstr;

	mrs_natural nControls;
	is >> nControls;

	for (i=0; i < nControls; i++)
	{
		/* For each control there are at least three lines:
		 * # ctrlname = ctrlvalue
		 * # LinksTo = nLinks
		 * # LinkedFrom = nLinks
		 * First, we get the cname and, from it, extract the type:
		 */
		is >> skipstr;
		is >> cname;

		string ctype;
		ctype = cname.substr(0, cname.rfind("/", cname.length()));

		iter = controls_.find(cname);

		is >> skipstr;
		// Now, based on the type, we extract the value
		if (ctype == rstr)
		{
		  is >> rcvalue;
		  if (iter == controls_.end())
		    addControl(cname, rcvalue);
		  else
		    updControl(cname, rcvalue);
		}
		else if (ctype == sstr)
		{
		  getline(is, scvalue);  // getline is used to include spaces in strings 
		  scvalue = scvalue.substr(1, scvalue.length()); // strip leading space 

		  if (scvalue == "MARSYAS_EMPTYSTRING")
		    scvalue = "";
		  if (iter == controls_.end())
		    addControl(cname, scvalue);
		  else
		    updControl(cname, scvalue);
		}
		else if (ctype == nstr)
		{
			is >> ncvalue;
			if (iter == controls_.end())
				addControl(cname, ncvalue);
			else
				updControl(cname, ncvalue);
		}
		else if (ctype == bstr)
		{
			is >> bcvalue;
			if (iter == controls_.end())
				addControl(cname, bcvalue);
			else
				updControl(cname, bcvalue);
		}
		else if (ctype == vstr)
		{
			realvec vcvalue;
			is >> vcvalue;
			if (iter == controls_.end())
				addControl(cname, vcvalue);
			else
				updControl(cname, vcvalue);
		}
		else
		{
			// if the ctype wasn't recognized as a base type
			// it's a control type. add a new control pointer
			MarControlPtr ctrl = MarControlManager::getManager()->createFromStream(ctype, is);
			if (iter == controls_.end())
				addControl(cname, ctrl);
			else
				updControl(cname, ctrl);
		}

		/*
		 * Now we read in the LinksTo and LinksFrom lines
		 */

		//clean all links for current control (if any)
		MarControlPtr curCtrl = getControlLocal(cname);
		curCtrl->unlinkFromAll();

		// read links
		int nLinks;
		string linkto;
		string linkedfrom;

		// Next line looks like:
		//# LinksTo = nlinks
		is >> skipstr >> skipstr >> skipstr;
		is >> nLinks;

		//relink
		if (nLinks > 0)
		{
			//read link absolute path
			is >> skipstr >> linkto;
			linkControl(cname, linkto);
		}

		// Next line looks like:
		//# LinksFrom = nlinks
		is >> skipstr >> skipstr >> skipstr;
		is >> nLinks;

		//relink
		/* If there are any LinksFrom, there will be one line for each.
		 * those lines look like:
		 * # /Series/net/Classifier/cl/ZeroRClassifier/zerorcl/mrs_natural/nClasses
		 * ie. hash sign, followed by a path
		 */
		for (mrs_natural link=0; link < nLinks; ++link)
		{
			//read link absolute path
			is >> skipstr;
			is >> linkedfrom;

			//look for control in the entire network
			MarControlPtr ctrl = getControl(linkedfrom, true, true);
			//if found, just link it to this control.
			//if not, this link will be done when the sibling or child MarSystem
			//owning the missing control is created
			if (!ctrl.isInvalid())
			{
				//ctrl->getMarSystem()->linkControl(linkedfrom, cname);
				ctrl->linkTo(curCtrl);
			}
		}
	}
	return is;
}

ostream&
Marsyas::operator<< (ostream& o, MarSystem& sys)
{
	sys.put(o);
	return o;
}

istream&
Marsyas::operator>> (istream& is, MarSystem& sys)
{
	sys.put(is);
	return is;
}

ostream&
Marsyas::operator<< (ostream& o, const map<string,MarControlPtr>& c)
{
	//lock map<string,MarControlPtr>& c for read? [?]
	o << "# MarControls = " << c.size() << endl;
	map<string, MarControlPtr>::const_iterator iter;
	for (iter=c.begin(); iter != c.end(); ++iter)
	{
		o << "# " << iter->first << " = " << iter->second << endl;
	}
	return o;
}



/**
 * \brief Helper function for adding a prefix to each of the observation names.
 *
 * \ingroup String
 *
 * \param observationNames string of observation names (comma separated)
 * \param prefix the prefix the prepend to all the observation names.
 * \return new comma separated observation name string
 *
 * \note Comma separated observation names in Marsyas use/expect a
 * trailing comma after the last item, this function follows this
 * convention.
 *
 * \todo Use this function in more places (e.g. search for occurrences of 'find(",")').
 */
mrs_string
Marsyas::obsNamesAddPrefix(mrs_string observationNames, mrs_string prefix)
{
	ostringstream oss;
	size_t startPos = 0, endPos=0;
	while ((endPos = observationNames.find(",", startPos)) != string::npos)
	{
		// Extract the observation name.
		mrs_string name = observationNames.substr(startPos, endPos-startPos);
		oss << prefix << name << ",";
		// Update the start position for the next name.
		startPos = endPos + 1;
	}
	return oss.str();
}


/**
 * \brief Helper function for splitting a string.
 *
 * \ingroup String
 *
 * \param input the string to split.
 * \param delimiter the string to split on.
 * \return a vector of strings.
 *
 * \note Comma separated observation names in Marsyas use/expect a
 * trailing comma after the last item. However, this function
 * follows the more traditional convention where the delimiter
 * is only between items. So when using this function for splitting
 * an observation names string, there will be an empty string
 * at the end of the returned list.
 *
 * \todo Use this function in more places (e.g. search for occurrences of 'find(",")').
 */
vector<mrs_string>
Marsyas::stringSplit(mrs_string input, mrs_string delimiter)
{
	vector<mrs_string> itemList;
	size_t startPos = 0, endPos=0;
	// Keep searching for the delimiter.
	while ((endPos = input.find(delimiter, startPos)) != string::npos)
	{
		// Get the current item.
		mrs_string item = input.substr(startPos, endPos - startPos);
		// Store it
		itemList.push_back(item);
		// Update the start position for the next name.
		startPos = endPos + delimiter.size();
	}
	// And the last item
	itemList.push_back(input.substr(startPos, input.size() - startPos));
	return itemList;
}

//**************************************************************************
//	MARSYAS_QT only methods
//**************************************************************************
#ifdef MARSYAS_QT

QMainWindow*
MarSystem::getMarSystemNetworkGUI(QWidget* parent, Qt::WFlags f)
{
	//if a network viewer already exists (i.e. is being displayed)
	//close it first in order to allow creating a new one
	//(this avoids any container window from being "empty")
	if (msysNetGUI_)
		return NULL;

	//create the Dialog
	msysNetGUI_ = new MarSystemNetworkGUI(this, parent, f);
	msysNetGUI_->setObjectName("MarSystemNetworkGUI");
	//string prefix = prefix_.substr(0, prefix_.length()-1);//remove trailing "/"
	msysNetGUI_->setWindowTitle(QString::fromStdString(prefix_) + " network");

	connect(msysNetGUI_, SIGNAL(destroyed(QObject*)),
	        this, SLOT(GUIdestroyed(QObject*)));

	//if no parent widget is specified, open the controls dialog
	//as an independent window and return a NULL pointer so it can
	//not be deleted by mistake. The MarSystemControlsGUI class, when
	//created without a parent deletes itself on close.
	if (!parent)
	{
		msysNetGUI_->setAttribute(Qt::WA_DeleteOnClose, true);
		msysNetGUI_->show();
		return NULL;
	}
	else
		return msysNetGUI_;
}

QMainWindow*
MarSystem::getMATLABeditorGUI(QWidget* parent, Qt::WFlags f)
{
#ifdef MARSYAS_MATLAB

	//if a MATLAB editor already exists (i.e. is being displayed)
	//close it first in order to allow creating a new one
	//(this avoids any container window from being "empty")
	if (MATLABeditorGUI_)
		return NULL;

	//create the Dialog
	MATLABeditorGUI_ = new MATLABeditorGUI(MATLABscript_,parent, f);
	//string path = prefix_.substr(0, absPath_.length()-1);//remove trailing "/"
	MATLABeditorGUI_->setWindowTitle(QString::fromStdString(absPath_));
	MATLABeditorGUI_->setObjectName("MATLABeditorGUI");

	connect(MATLABeditorGUI_, SIGNAL(scriptChanged(std::string)),
	        this, SLOT(setMATLABscript(std::string)));

	// 	Q_ASSERT(
	// 		connect(this, SIGNAL(processed()),
	// 						MATLABeditorGUI_, SLOT(updateOutputDisplay()))
	// 		);

	connect(MATLABeditorGUI_, SIGNAL(destroyed(QObject*)),
	        this, SLOT(GUIdestroyed(QObject*)));

	//if no parent widget is specified, open the controls dialog
	//as an independent window and return a NULL pointer so it can
	//not be deleted by mistake. The MarSystemControlsGUI class, when
	//created without a parent deletes itself on close.
	if (!parent)
	{
		MATLABeditorGUI_->setAttribute(Qt::WA_DeleteOnClose, true);
		MATLABeditorGUI_->show();
		return NULL;
	}
	else
		return MATLABeditorGUI_;

#else //MARSYAS_MATLAB
	MRSWARN("MarSystem::getMATLABeditor(): Marsyas not built with MATLAB engine support!");
	return NULL;
#endif //MARSYAS_MATLAB
}

QMainWindow*
MarSystem::getControlsGUI(QWidget* parent, Qt::WFlags f)
{
	//create a MarControls editor GUI for this MarSystem
	MarSystemControlsGUI* controlsGUI = MarGUIManager::getControlsGUI(this, parent, f);
	controlsGUI->setWindowTitle(QString::fromStdString(absPath_));
	controlsGUI->setObjectName("controlsGUI_" + QDateTime::currentDateTime().toString(Qt::ISODate));

	//store this control in the active GUIs list
	activeControlsGUIs_[controlsGUI->objectName()] = controlsGUI;

	//connect signal sent by controlsGUI whenever a control value is changed
	//by the user
	// 	Q_ASSERT(
	// 		connect(controlsGUI_, SIGNAL(controlChanged(std::string, MarControlPtr)),
	// 		this, SLOT(updControl(std::string, MarControlPtr)))
	// 		);

	//connect a signal to the controlsGUI to update any control
	//whose value was modified elsewhere.
	//This would in theory create an infinite loop anytime a control was changed
	//using the GUI, but the MarSystemControlsGUI class provides a mechanism for
	//avoiding that (See MarSystemControlsGUI.cpp).
	connect(this, SIGNAL(controlChanged(MarControl*)),
	        controlsGUI, SLOT(updControl(MarControl*)));

	//connect the controlsGUI destroyed signal to a slot so we can detect
	//when the controls editor was closed/destroyed
	connect(controlsGUI, SIGNAL(destroyed(QObject*)),
	        this, SLOT(GUIdestroyed(QObject*)));

	//if no parent widget is specified, open the controls dialog
	//as an independent window and return a NULL pointer so it can
	//not be deleted by mistake. The MarSystemControlsGUI class, when
	//created without a parent deletes itself on close.
	if (!parent)
	{
		controlsGUI->setAttribute(Qt::WA_DeleteOnClose, true);
		controlsGUI->show();
		return NULL;
	}
	else
		return controlsGUI;
}

QMainWindow*
MarSystem::getDataGUI(QWidget* parent, Qt::WFlags f)
{
	//to be further defined...
	//...
	return NULL;
}

void
MarSystem::GUIdestroyed(QObject *obj)
{
	//check if the Qt object destroyed was the network viewer...
	if (obj->objectName() == "MarSystemNetworkGUI")
	{
		msysNetGUI_ = NULL;
		return;
	}
	//check if the Qt object destroyed was the MATLAB editor...
	if (obj->objectName() == "MATLABeditorGUI")
	{
		MATLABeditorGUI_ = NULL;
		return;
	}
	//remove destroyed GUI objects from the active lists
	//so they are not deleted again at the destructor
	activeControlsGUIs_.remove(obj->objectName());
	activeDataGUIs_.remove(obj->objectName());
}

// #ifdef MARSYAS_QT
// void
// MarControls::emitControlChanged(MarControlPtr* control)
// {
// 	//only bother calling MarSystem's controlChanged signal
// 	//if there is a GUI currently active(i.e. being displayed)
// 	//=> more efficient! [!]
// 	if(msys_->controlsGUI_ || msys_->dataGUI_)//possible because this class is friend of MarSystem //[!]
// 	{
// 		Q_ASSERT(QMetaObject::invokeMethod(msys_, "controlChanged", Qt::AutoConnection,
// 			Q_ARG(MarControlPtr*, control)));
// 	}
// }
// #endif //MARSYAS_QT

#endif //MARSYAS_QT
