/* Suit.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SUIT_H_
#define SUIT_H_


#include "Body.h"
#include "Bodymod.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

class DataNode;
class DataWriter;

class Phrase;
class PlayerInfo;
class Visual;



// Class representing a suit (either a model for sale or an instance of it). A
// suit's information can be saved to a file, so that it can later be read back
// in exactly the same state. The same class is used for the player's suit as
// for all other suits, so their capabilities are exactly the same  within the
// limits of what the AI knows how to command them to do.
class Suit : public Body, public std::enable_shared_from_this<Suit> {
public:
	// These are all the possible category strings for suits.
	static const std::vector<std::string> CATEGORIES;

	
	
public:

	Suit() = default;
	// Construct and Load() at the same time.
	Suit(const DataNode &node);
	
	// Load data for a type of suit:
	void Load(const DataNode &node);
	// When loading a suit, some of the bodymods it lists may not have been
	// loaded yet. So, wait until everything has been loaded, then call this.
	void FinishLoading(bool isNewInstance);
	// Save a full description of this suit, as currently configured.
	void Save(DataWriter &out) const;
	
	// Get the name of this particular suit.
	const std::string &Name() const;
	
	// Get the name of this model of suit.
	const std::string &ModelName() const;
	const std::string &PluralModelName() const;
	// Get the generic noun (e.g. "suit") to be used when describing this suit.
	const std::string &Noun() const;
	// Get this suit's description.
	const std::string &Description() const;
	// Get the suityard thumbnail for this suit.
	const Sprite *Thumbnail() const;
	// Get this suit's cost.
	int64_t Cost() const;
	int64_t ChassisCost() const;

	void SetName(const std::string &name);

	void SetIsSpecial(bool special = true);
	bool IsSpecial() const;
	
	// If a suit belongs to the player, the player can give it commands.
	void SetIsYours(bool yours = true);
	bool IsYours() const;


	// Check the status of this suit.
	bool IsCapturable() const;
	// Get this suit's custom swizzle.
	int CustomSwizzle() const;

	void Recharge(bool atSpaceport = true);
	// Check if this suit is able to give the given suit enough fuel to jump.

	// Check if this is a suit that can be used as a flagsuit.
	bool CanBeFlagsuit() const;

	// Get the current attributes of this suit.
	const Bodymod &Attributes() const;
	// Get the attributes of this suit chassis before any bodymods were added.
	const Bodymod &BaseAttributes() const;
	// Get the list of all bodymods installed in this suit.
	const std::map<const Bodymod *, int> &Bodymods() const;
	// Find out how many bodymods of the given type this suit contains.
	int BodymodCount(const Bodymod *bodymod) const;
	// Add or remove bodymods. (To remove, pass a negative number.)
	void AddBodymod(const Bodymod *bodymod, int count);

private:
	/* Protected member variables of the Body class:
	Point position;
	Point velocity;
	Angle angle;
	double zoom;
	int swizzle;
	const Government *government;
	*/
	
	// Characteristics of the chassis:
	const Suit *base = nullptr;
	std::string modelName;
	std::string pluralModelName;
	std::string noun;
	std::string description;
	const Sprite *thumbnail = nullptr;
	// Characteristics of this particular suit:
	std::string name;
	int forget = 0;
	bool isSpecial = false;
	bool isYours = false;
	bool isCapturable = true;
	int customSwizzle = -1;

	// Installed bodymods, cargo, etc.:
	Bodymod attributes;
	Bodymod baseAttributes;
	bool addAttributes = false;
	std::map<const Bodymod *, int> bodymods;
	std::map<const Bodymod *, int> equipped;
};



#endif
