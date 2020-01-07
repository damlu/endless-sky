/* Suit.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Suit.h"

#include "Audio.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Files.h"
#include "Format.h"
#include "GameData.h"
#include "Mask.h"
#include "Messages.h"
#include "Phrase.h"
#include "PlayerInfo.h"
#include "Preferences.h"
#include "Random.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "System.h"
#include "Visual.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {
}

const vector<string> Suit::CATEGORIES = {
	"Transport",
	"Light Freighter",
	"Heavy Freighter",
	"Interceptor",
	"Light Warsuit",
	"Medium Warsuit",
	"Heavy Warsuit",
	"Fighter",
	"Drone"
};



// Construct and Load() at the same time.
Suit::Suit(const DataNode &node)
{
	Load(node);
}



void Suit::Load(const DataNode &node)
{
	if(node.Size() >= 2)
	{
		modelName = node.Token(1);
		pluralModelName = modelName + 's';
	}
	if(node.Size() >= 3)
		base = GameData::Suits().Get(modelName);

	equipped.clear();
	
	// Note: I do not clear the attributes list here so that it is permissible
	// to override one suit definition with another.
	bool hasBodymods = false;
	bool hasDescription = false;
	bool hasAnatomyDescription = false;
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool add = (key == "add");
		if(add && (child.Size() < 2 || child.Token(1) != "attributes"))
		{
			child.PrintTrace("Skipping invalid use of 'add' with " + (child.Size() < 2
					? "no key." : "key: " + child.Token(1)));
			continue;
		}
		if(key == "sprite")
			LoadSprite(child);
		else if(child.Token(0) == "thumbnail" && child.Size() >= 2)
			thumbnail = SpriteSet::Get(child.Token(1));
		else if(key == "name" && child.Size() >= 2)
			name = child.Token(1);
		else if(key == "plural" && child.Size() >= 2)
			pluralModelName = child.Token(1);
		else if(key == "noun" && child.Size() >= 2)
			noun = child.Token(1);
		else if(key == "swizzle" && child.Size() >= 2)
			customSwizzle = child.Value(1);
		else if(key == "attributes" || add)
		{
			if(!add)
				baseAttributes.Load(child);
			else
			{
				addAttributes = true;
				attributes.Load(child);
			}
		}
		else if(key == "uncapturable")
			isCapturable = false;
		else if(key == "bodymods")
		{
			if(!hasBodymods)
			{
				bodymods.clear();
				hasBodymods = true;
			}
			for(const DataNode &grand : child)
			{
				int count = (grand.Size() >= 2) ? grand.Value(1) : 1;
				if(count > 0)
					bodymods[GameData::Bodymods().Get(grand.Token(0))] += count;
				else
					grand.PrintTrace("Skipping invalid bodymod count:");
			}
		}
		else if(key == "description" && child.Size() >= 2)
		{
			if(!hasDescription)
			{
				description.clear();
				hasDescription = true;
			}
			description += child.Token(1);
			description += '\n';
		}
		else if(key == "anatomy description" && child.Size() >= 2)
		{
			if(!hasAnatomyDescription)
			{
				anatomyDescription.clear();
				hasAnatomyDescription = true;
			}
			anatomyDescription += child.Token(1);
			anatomyDescription += '\n';
		}
		else if(key != "actions")
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



// When loading a suit, some of the bodymods it lists may not have been
// loaded yet. So, wait until everything has been loaded, then call this.
void Suit::FinishLoading(bool isNewInstance)
{
	// All copies of this suit should save pointers to the "explosion" weapon
	// definition stored safely in the suit model, which will not be destroyed
	// until GameData is when the program quits. Also copy other attributes of
	// the base model if no overrides were given.
	if(GameData::Suits().Has(modelName))
	{
		const Suit *model = GameData::Suits().Get(modelName);
		if(pluralModelName.empty())
			pluralModelName = model->pluralModelName;
		if(noun.empty())
			noun = model->noun;
		if(!thumbnail)
			thumbnail = model->thumbnail;
	}
	
	// If this suit has a base class, copy any attributes not defined here.
	// Exception: uncapturable and "never disabled" flags don't carry over.
	if(base && base != this)
	{
		if(!GetSprite())
			reinterpret_cast<Body &>(*this) = *base;
		if(customSwizzle == -1)
			customSwizzle = base->CustomSwizzle();
		if(baseAttributes.Attributes().empty())
			baseAttributes = base->baseAttributes;
		if(bodymods.empty())
			bodymods = base->bodymods;
		if(description.empty())
			description = base->description;
		if(anatomyDescription.empty())
			description = base->anatomyDescription;
	}

	if(addAttributes)
	{
		// Store attributes from an "add attributes" node in the suit's
		// baseAttributes so they can be written to the save file.
		baseAttributes.Add(attributes);
		addAttributes = false;
	}
	// Add the attributes of all your bodymods to the suit's base attributes.
	attributes = baseAttributes;
	for(const auto &it : bodymods)
	{
		if(it.first->Name().empty())
		{
			Files::LogError("Unrecognized bodymod in " + modelName + " \"" + name + "\"");
			continue;
		}
		attributes.Add(*it.first, it.second);
		// Some suit variant definitions do not specify which weapons
		// are placed in which hardpoint. Add any weapons that are not
		// yet installed to the suit's armament.
		if(it.first->IsWeapon())
		{
			int count = it.second;
			auto eit = equipped.find(it.first);
			if(eit != equipped.end())
				count -= eit->second;
		}
	}

	equipped.clear();

	
	// If this suit is being instantiated for the first time, make sure its
	// crew, fuel, etc. are all refilled.
	if(isNewInstance)
		Recharge(true);

	
	// Issue warnings if this suit has negative bodymod, cargo, weapon, or engine capacity.
	string warning;
	// for(const string &attr : set<string>{"bodymod space", "cargo space", "weapon capacity", "engine capacity"})
	for(const string &attr : set<string>{"bodymod space"})
	{
		double val = attributes.Get(attr);
		if(val < 0)
			warning += attr + ": " + Format::Number(val) + "\n";
	}
	if(!warning.empty())
	{
		// This check is mostly useful for variants and stock suits, which have
		// no names. Print the bodymods to facilitate identifying this suit definition.
		string message = (!name.empty() ? "Suit \"" + name + "\" " : "") + "(" + modelName + "):\n";
		ostringstream bodymodNames("bodymods:\n");
		for(const auto &it : bodymods)
			bodymodNames << '\t' << it.second << " " + it.first->Name() << endl;
		Files::LogError(message + warning + bodymodNames.str());
	}

}



// Save a full description of this suit, as currently configured.
void Suit::Save(DataWriter &out) const
{
	out.Write("suit", modelName);
	out.BeginChild();
	{
		out.Write("name", name);
		if(pluralModelName != modelName + 's')
			out.Write("plural", pluralModelName);
		if(!noun.empty())
			out.Write("noun", noun);
		SaveSprite(out);

		if(!isCapturable)
			out.Write("uncapturable");
		if(customSwizzle >= 0)
			out.Write("swizzle", customSwizzle);
		
		out.Write("attributes");
		out.BeginChild();
		{
			out.Write("category", baseAttributes.Category());
			out.Write("cost", baseAttributes.Cost());
			for(const auto &it : baseAttributes.Attributes())
				if(it.second)
					out.Write(it.first, it.second);
		}
		out.EndChild();
		
		out.Write("bodymods");
		out.BeginChild();
		{
			for(const auto &it : bodymods)
				if(it.first && it.second)
				{
					if(it.second == 1)
						out.Write(it.first->Name());
					else
						out.Write(it.first->Name(), it.second);
				}
		}
		out.EndChild();

	}
	out.EndChild();
}



const string &Suit::Name() const
{
	return name;
}



const string &Suit::ModelName() const
{
	return modelName;
}



const string &Suit::PluralModelName() const
{
	return pluralModelName;
}



// Get the generic noun (e.g. "suit") to be used when describing this suit.
const string &Suit::Noun() const
{
	static const string SUIT = "suit";
	return noun.empty() ? SUIT : noun;
}



// Get this suit's description.
const string &Suit::Description() const
{
	return description;
}

// Get this suit's description.
const string &Suit::AnatomyDescription() const
{
	return anatomyDescription;
}


// Get the suityard thumbnail for this suit.
const Sprite *Suit::Thumbnail() const
{
	return thumbnail;
}



// Get this suit's cost.
int64_t Suit::Cost() const
{
	return attributes.Cost();
}



// Get the cost of this suit's chassis, with no bodymods installed.
int64_t Suit::ChassisCost() const
{
	return baseAttributes.Cost();
}



// Set the name of this particular suit.
void Suit::SetName(const string &name)
{
	this->name = name;
}

void Suit::SetIsSpecial(bool special)
{
	isSpecial = special;
}



bool Suit::IsSpecial() const
{
	return isSpecial;
}



void Suit::SetIsYours(bool yours)
{
	isYours = yours;
}



bool Suit::IsYours() const
{
	return isYours;
}

bool Suit::IsCapturable() const
{
	return isCapturable;
}


// Get this suit's custom swizzle.
int Suit::CustomSwizzle() const
{
	return customSwizzle;
}



// Recharge and repair this suit (e.g. because it has landed).
void Suit::Recharge(bool atSpaceport)
{

}



//// Convert this suit from one government to another, as a result of boarding
//// actions (if the player is capturing) or player death (poor decision-making).
//void Suit::WasCaptured(const shared_ptr<Ship> &capturer)
//{
//
//	// This suit behaves like its new parent does.
//	isSpecial = capturer->isSpecial;
//	isYours = capturer->isYours;
//}


// Check if this is a suit that can be used as a flagsuit.
bool Suit::CanBeFlagsuit() const
{
	return true;
}



const Bodymod &Suit::Attributes() const
{
	return attributes;
}



const Bodymod &Suit::BaseAttributes() const
{
	return baseAttributes;
}



// Get bodymod information.
const map<const Bodymod *, int> &Suit::Bodymods() const
{
	return bodymods;
}



int Suit::BodymodCount(const Bodymod *bodymod) const
{
	auto it = bodymods.find(bodymod);
	return (it == bodymods.end()) ? 0 : it->second;
}



// Add or remove bodymods. (To remove, pass a negative number.)
void Suit::AddBodymod(const Bodymod *bodymod, int count)
{
	if(bodymod && count)
	{
		auto it = bodymods.find(bodymod);
		if(it == bodymods.end())
			bodymods[bodymod] = count;
		else
		{
			it->second += count;
			if(!it->second)
				bodymods.erase(it);
		}
		attributes.Add(*bodymod, count);
	}
}


