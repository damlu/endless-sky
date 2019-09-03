/* Bodymod.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Bodymod.h"

#include "Audio.h"
#include "Body.h"
#include "DataNode.h"
#include "Effect.h"
#include "GameData.h"
#include "SpriteSet.h"

#include <cmath>

using namespace std;

namespace {
	const double EPS = 0.0000000001;
}

const vector<string> Bodymod::CATEGORIES = {
	"Guns",
	"Turrets",
	"Secondary Weapons",
	"Ammunition",
	"Systems",
	"Power",
	"Engines",
	"Hand to Hand",
	"Special"
};



void Bodymod::Load(const DataNode &node)
{
	if(node.Size() >= 2)
	{
		name = node.Token(1);
		pluralName = name + 's';
	}
	
	for(const DataNode &child : node)
	{
		if(child.Token(0) == "category" && child.Size() >= 2)
			category = child.Token(1);
		else if(child.Token(0) == "plural" && child.Size() >= 2)
			pluralName = child.Token(1);
		else if(child.Token(0) == "flotsam sprite" && child.Size() >= 2)
			flotsamSprite = SpriteSet::Get(child.Token(1));
		else if(child.Token(0) == "thumbnail" && child.Size() >= 2)
			thumbnail = SpriteSet::Get(child.Token(1));
		else if(child.Token(0) == "weapon")
			LoadWeapon(child);
		else if(child.Token(0) == "description" && child.Size() >= 2)
		{
			description += child.Token(1);
			description += '\n';
		}
		else if(child.Token(0) == "cost" && child.Size() >= 2)
			cost = child.Value(1);
		else if(child.Token(0) == "mass" && child.Size() >= 2)
			mass = child.Value(1);
		else if(child.Token(0) == "licenses")
		{
			for(const DataNode &grand : child)
				licenses.push_back(grand.Token(0));
		}
		else if(child.Size() >= 2)
			attributes[child.Token(0)] = child.Value(1);
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



const string &Bodymod::Name() const
{
	return name;
}



const string &Bodymod::PluralName() const
{
	return pluralName;
}



const string &Bodymod::Category() const
{
	return category;
}



const string &Bodymod::Description() const
{
	return description;
}



// Get the licenses needed to purchase this bodymod.
const vector<string> &Bodymod::Licenses() const
{
	return licenses;
}



// Get the image to display in the bodymodder when buying this item.
const Sprite *Bodymod::Thumbnail() const
{
	return thumbnail;
}



double Bodymod::Get(const char *attribute) const
{
	return attributes.Get(attribute);
}



double Bodymod::Get(const string &attribute) const
{
	return Get(attribute.c_str());
}



const Dictionary &Bodymod::Attributes() const
{
	return attributes;
}



// Determine whether the given number of instances of the given bodymod can
// be added to a suit with the attributes represented by this instance. If
// not, return the maximum number that can be added.
int Bodymod::CanAdd(const Bodymod &other, int count) const
{
	for(const auto &at : other.attributes)
	{
		double value = Get(at.first);
		// Allow for rounding errors:
		if(value + at.second * count < -EPS)
			count = value / -at.second + EPS;
	}
	
	return count;
}



// For tracking a combination of bodymods in a suit: add the given number of
// instances of the given bodymod to this bodymod.
void Bodymod::Add(const Bodymod &other, int count)
{
	cost += other.cost * count;
	mass += other.mass * count;
	for(const auto &at : other.attributes)
	{
		attributes[at.first] += at.second * count;
		if(fabs(attributes[at.first]) < EPS)
			attributes[at.first] = 0.;
	}

}



// Modify this bodymod's attributes.
void Bodymod::Set(const char *attribute, double value)
{
	attributes[attribute] = value;
}

// Get the sprite this bodymod uses when dumped into space.
const Sprite *Bodymod::FlotsamSprite() const
{
	return flotsamSprite;
}
