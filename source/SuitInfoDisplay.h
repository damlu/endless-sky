/* SuitInfoDisplay.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SUIT_INFO_DISPLAY_H_
#define SUIT_INFO_DISPLAY_H_

#include "ItemInfoDisplay.h"
#include "Bodymod.h"

#include <string>
#include <vector>

class Depreciation;
class Point;
class Suit;



// Class representing three panels of information about a given suit. One shows the
// suit's description, the second summarizes its attributes, and the third lists
// all bodymods currently installed in the suit. This is used for the suityard, for
// showing changes to your suit as you add upgrades, for scanning other suits, etc.
class SuitInfoDisplay : public ItemInfoDisplay {
public:
	SuitInfoDisplay() = default;
	SuitInfoDisplay(const Suit &suit, const Depreciation &depreciation, int day);
	
	// Call this every time the suit changes.
	void Update(const Suit &suit, const Depreciation &depreciation, int day);
	
	// Provided by ItemInfoDisplay:
	// int PanelWidth();
	// int MaximumHeight() const;
	// int DescriptionHeight() const;
	// int AttributesHeight() const;
	int BodymodsHeight() const;
	int SaleHeight() const;
	
	// Provided by ItemInfoDisplay:
	// void DrawDescription(const Point &topLeft) const;
	virtual void DrawAttributes(const Point &topLeft) const override;
	void DrawBodymods(const Point &topLeft) const;
	void DrawBodymodSlots(const Point &topLeft) const;
	void DrawSale(const Point &topLeft) const;

	static std::string GetAnatomyRating(Bodymod attributes, std::string attributeName);
	
	
private:
	void UpdateAttributes(const Suit &suit, const Depreciation &depreciation, int day);
	void UpdateBodymods(const Suit &suit, const Depreciation &depreciation, int day);
	void UpdateBodymodSlots(const Suit &suit);
	
	
private:
	std::vector<std::string> tableLabels;
	std::vector<std::string> energyTable;
	std::vector<std::string> heatTable;
	
	std::vector<std::string> bodymodLabels;
	std::vector<std::string> bodymodValues;
	int bodymodsHeight = 0;

	std::vector<std::string> bodymodSlotsLabels;
	std::vector<std::string> bodymodSlotsValues;
	int bodymodSlotsHeight = 0;


	std::vector<std::string> saleLabels;
	std::vector<std::string> saleValues;
	int saleHeight = 0;
};



#endif
