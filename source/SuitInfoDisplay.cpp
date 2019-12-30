/* SuitInfoDisplay.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SuitInfoDisplay.h"

#include "Color.h"
#include "Depreciation.h"
#include "FillShader.h"
#include "Format.h"
#include "GameData.h"
#include "Bodymod.h"
#include "Suit.h"
#include "Table.h"

#include <algorithm>
#include <map>
#include <sstream>

using namespace std;



SuitInfoDisplay::SuitInfoDisplay(const Suit &suit, const Depreciation &depreciation, int day)
{
	Update(suit, depreciation, day);
}



// Call this every time the suit changes.
void SuitInfoDisplay::Update(const Suit &suit, const Depreciation &depreciation, int day)
{
	UpdateDescription(suit.Description(), suit.Attributes().Licenses(), true);
	UpdateAttributes(suit, depreciation, day);
	UpdateBodymods(suit, depreciation, day);
	
	maximumHeight = max(descriptionHeight, max(attributesHeight, bodymodsHeight));
}



int SuitInfoDisplay::BodymodsHeight() const
{
	return bodymodsHeight;
}



int SuitInfoDisplay::SaleHeight() const
{
	return saleHeight;
}



// Draw each of the panels.
void SuitInfoDisplay::DrawAttributes(const Point &topLeft) const
{
	Point point = Draw(topLeft, attributeLabels, attributeValues);
	
	// Get standard colors to draw with.
	const Color &labelColor = *GameData::Colors().Get("medium");
	const Color &valueColor = *GameData::Colors().Get("bright");
	
	Table table;
	table.AddColumn(10, Table::LEFT);
	table.AddColumn(WIDTH - 90, Table::RIGHT);
	table.AddColumn(WIDTH - 10, Table::RIGHT);
	table.SetHighlight(0, WIDTH);
	table.DrawAt(point);
	table.DrawGap(10.);
	
	table.Advance();
//	table.Draw("energy", labelColor);
//	table.Draw("heat", labelColor);
	
	for(unsigned i = 0; i < tableLabels.size(); ++i)
	{
		CheckHover(table, tableLabels[i]);
		table.Draw(tableLabels[i], labelColor);
//		table.Draw(energyTable[i], valueColor);
//		table.Draw(heatTable[i], valueColor);
	}
}



void SuitInfoDisplay::DrawBodymods(const Point &topLeft) const
{
	Draw(topLeft, bodymodLabels, bodymodValues);
}



void SuitInfoDisplay::DrawSale(const Point &topLeft) const
{
	Draw(topLeft, saleLabels, saleValues);
	
	const Color &color = *GameData::Colors().Get("medium");
	FillShader::Fill(topLeft + Point(.5 * WIDTH, saleHeight + 8.), Point(WIDTH - 20., 1.), color);
}



void SuitInfoDisplay::UpdateAttributes(const Suit &suit, const Depreciation &depreciation, int day)
{
	bool isGeneric = suit.Name().empty();
	
	attributeLabels.clear();
	attributeValues.clear();
	attributesHeight = 20;
	
	const Bodymod &attributes = suit.Attributes();
	
	int64_t fullCost = suit.Cost();
	int64_t depreciated = depreciation.Value(suit, day);
	if(depreciated == fullCost)
		attributeLabels.push_back("cost:");
	else
	{
		ostringstream out;
		out << "cost (" << (100 * depreciated) / fullCost << "%):";
		attributeLabels.push_back(out.str());
	}
	attributeValues.push_back(Format::Credits(depreciated));
	attributesHeight += 20;
	
	attributeLabels.push_back(string());
	attributeValues.push_back(string());
	attributesHeight += 10;
	attributeLabels.push_back("cum capacity:");
	attributeValues.push_back(Format::Number(attributes.Get("cum capacity")));
	attributesHeight += 20;

	attributeLabels.push_back("milk capacity:");
	attributeValues.push_back(Format::Number(attributes.Get("milk capacity")));
	attributesHeight += 20;



//	double emptyMass = suit.Mass();
//	attributeLabels.push_back(isGeneric ? "mass with no cargo:" : "mass:");
//	attributeValues.push_back(Format::Number(emptyMass));
//	attributesHeight += 20;
//	attributeLabels.push_back(isGeneric ? "cargo space:" : "cargo:");
//	if(isGeneric)
//		attributeValues.push_back(Format::Number(attributes.Get("cargo space")));
//	else
//		attributeValues.push_back(Format::Number(suit.Cargo().Used())
//			+ " / " + Format::Number(attributes.Get("cargo space")));
//	attributesHeight += 20;
//	attributeLabels.push_back("required crew / bunks:");
//	attributeValues.push_back(Format::Number(suit.RequiredCrew())
//		+ " / " + Format::Number(attributes.Get("bunks")));
//	attributesHeight += 20;
//	attributeLabels.push_back(isGeneric ? "fuel capacity:" : "fuel:");
//	double fuelCapacity = attributes.Get("fuel capacity");
//	if(isGeneric)
//		attributeValues.push_back(Format::Number(fuelCapacity));
//	else
//		attributeValues.push_back(Format::Number(suit.Fuel() * fuelCapacity)
//			+ " / " + Format::Number(fuelCapacity));
//	attributesHeight += 20;
	
//	double fullMass = emptyMass + (isGeneric ? attributes.Get("cargo space") : suit.Cargo().Used());
//	isGeneric &= (fullMass != emptyMass);
//	double forwardThrust = attributes.Get("thrust") ? attributes.Get("thrust") : attributes.Get("afterburner thrust");
//	attributeLabels.push_back(string());
//	attributeValues.push_back(string());
//	attributesHeight += 10;
//	attributeLabels.push_back(isGeneric ? "movement, full / no cargo:" : "movement:");
//	attributeValues.push_back(string());
//	attributesHeight += 20;
//	attributeLabels.push_back("max speed:");
//	attributeValues.push_back(Format::Number(60. * forwardThrust / attributes.Get("drag")));
//	attributesHeight += 20;
//
//	attributeLabels.push_back("acceleration:");
//	if(!isGeneric)
//		attributeValues.push_back(Format::Number(3600. * forwardThrust / fullMass));
//	else
//		attributeValues.push_back(Format::Number(3600. * forwardThrust / fullMass)
//			+ " / " + Format::Number(3600. *forwardThrust / emptyMass));
//	attributesHeight += 20;
//
//	attributeLabels.push_back("turning:");
//	if(!isGeneric)
//		attributeValues.push_back(Format::Number(60. * attributes.Get("turn") / fullMass));
//	else
//		attributeValues.push_back(Format::Number(60. * attributes.Get("turn") / fullMass)
//			+ " / " + Format::Number(60. * attributes.Get("turn") / emptyMass));
//	attributesHeight += 20;
	
	// Find out how much bodymod, engine, and weapon space the chassis has.
	map<string, double> chassis;
//	static const vector<string> NAMES = {
//		"bodymod space free:", "bodymod space",
//		"    weapon capacity:", "weapon capacity",
//		"    engine capacity:", "engine capacity",
//		"gun ports free:", "gun ports",
//		"turret mounts free:", "turret mounts"
//	};
    static const vector<string> NAMES = {
		"bodymod space free:", "bodymod space",
		"leg slots free:", "leg slots",
		"butt slots free:", "butt slots",
		"crotch slots free:", "crotch slots",
		"chest slots free:", "chest slots",
		"    nipple slots free:", "nipple slots",
		"arm slots free:", "arm slots",
		"head slots free:", "head slots"
    };
	for(unsigned i = 1; i < NAMES.size(); i += 2)
		chassis[NAMES[i]] = attributes.Get(NAMES[i]);
	for(const auto &it : suit.Bodymods())
		for(auto &cit : chassis)
			cit.second -= it.second * it.first->Get(cit.first);
	
	attributeLabels.push_back(string());
	attributeValues.push_back(string());
	attributesHeight += 10;
	for(unsigned i = 0; i < NAMES.size(); i += 2)
	{
		attributeLabels.push_back(NAMES[i]);
		attributeValues.push_back(Format::Number(attributes.Get(NAMES[i + 1]))
			+ " / " + Format::Number(chassis[NAMES[i + 1]]));
		attributesHeight += 20;
	}
	
//	if(suit.BaysFree(false))
//	{
//		attributeLabels.push_back("drone bays:");
//		attributeValues.push_back(to_string(suit.BaysFree(false)));
//		attributesHeight += 20;
//	}
//	if(suit.BaysFree(true))
//	{
//		attributeLabels.push_back("fighter bays:");
//		attributeValues.push_back(to_string(suit.BaysFree(true)));
//		attributesHeight += 20;
//	}
//
	tableLabels.clear();
//	energyTable.clear();
//	heatTable.clear();
	// Skip a spacer and the table header.
//	attributesHeight += 30;
//
//	tableLabels.push_back("idle:");
//	energyTable.push_back(Format::Number(
//		60. * (attributes.Get("energy generation")
//			+ attributes.Get("solar collection")
//			+ attributes.Get("fuel energy")
//			- attributes.Get("energy consumption")
//			- attributes.Get("cooling energy"))));
//	double efficiency = suit.CoolingEfficiency();
//	heatTable.push_back(Format::Number(
//		60. * (attributes.Get("heat generation")
//			+ attributes.Get("fuel heat")
//			- efficiency * (attributes.Get("cooling") + attributes.Get("active cooling")))));
//	attributesHeight += 20;
//	tableLabels.push_back("moving:");
//	energyTable.push_back(Format::Number(
//		-60. * (max(attributes.Get("thrusting energy"), attributes.Get("reverse thrusting energy"))
//			+ attributes.Get("turning energy")
//			+ attributes.Get("afterburner energy"))));
//	heatTable.push_back(Format::Number(
//		60. * (max(attributes.Get("thrusting heat"), attributes.Get("reverse thrusting heat"))
//			+ attributes.Get("turning heat")
//			+ attributes.Get("afterburner heat"))));
//	attributesHeight += 20;
//	double firingEnergy = 0.;
//	double firingHeat = 0.;
//	for(const auto &it : suit.Bodymods())
//		if(it.first->IsWeapon() && it.first->Reload())
//		{
//			firingEnergy += it.second * it.first->FiringEnergy() / it.first->Reload();
//			firingHeat += it.second * it.first->FiringHeat() / it.first->Reload();
//		}
//	tableLabels.push_back("firing:");
//	energyTable.push_back(Format::Number(-60. * firingEnergy));
//	heatTable.push_back(Format::Number(60. * firingHeat));
//	attributesHeight += 20;
//	double shieldEnergy = attributes.Get("shield energy");
//	double hullEnergy = attributes.Get("hull energy");
//	tableLabels.push_back((shieldEnergy && hullEnergy) ? "shields / hull:" :
//		hullEnergy ? "repairing hull:" : "charging shields:");
//	energyTable.push_back(Format::Number(-60. * (shieldEnergy + hullEnergy)));
//	double shieldHeat = attributes.Get("shield heat");
//	double hullHeat = attributes.Get("hull heat");
//	heatTable.push_back(Format::Number(60. * (shieldHeat + hullHeat)));
//	attributesHeight += 20;
//	tableLabels.push_back("max:");
//	energyTable.push_back(Format::Number(attributes.Get("energy capacity")));
//	heatTable.push_back(Format::Number(60. * suit.HeatDissipation() * suit.MaximumHeat()));
	// Pad by 10 pixels on the top and bottom.
	attributesHeight += 30;
}



void SuitInfoDisplay::UpdateBodymods(const Suit &suit, const Depreciation &depreciation, int day)
{
	bodymodLabels.clear();
	bodymodValues.clear();
	bodymodsHeight = 20;
	
	map<string, map<string, int>> listing;
	for(const auto &it : suit.Bodymods())
		listing[it.first->Category()][it.first->Name()] += it.second;
	
	for(const auto &cit : listing)
	{
		// Pad by 10 pixels before each category.
		if(&cit != &*listing.begin())
		{
			bodymodLabels.push_back(string());
			bodymodValues.push_back(string());
			bodymodsHeight += 10;
		}
				
		bodymodLabels.push_back(cit.first + ':');
		bodymodValues.push_back(string());
		bodymodsHeight += 20;
		
		for(const auto &it : cit.second)
		{
			bodymodLabels.push_back(it.first);
			bodymodValues.push_back(to_string(it.second));
			bodymodsHeight += 20;
		}
	}
	
	
	int64_t totalCost = depreciation.Value(suit, day);
	int64_t chassisCost = depreciation.Value(GameData::Suits().Get(suit.ModelName()), day);
	saleLabels.clear();
	saleValues.clear();
	saleHeight = 20;
	
	saleLabels.push_back("This suit will sell for:");
	saleValues.push_back(string());
	saleHeight += 20;
	saleLabels.push_back("empty hull:");
	saleValues.push_back(Format::Credits(chassisCost));
	saleHeight += 20;
	saleLabels.push_back("  + bodymods:");
	saleValues.push_back(Format::Credits(totalCost - chassisCost));
	saleHeight += 5;
}
