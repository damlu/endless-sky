/* BodymodderPanel.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "BodymodderPanel.h"

#include "Color.h"
#include "Dialog.h"
#include "DistanceMap.h"
#include "FillShader.h"
#include "Font.h"
#include "FontSet.h"
#include "Format.h"
#include "GameData.h"
#include "Hardpoint.h"
#include "Bodymod.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "Suit.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

#include <algorithm>
#include <limits>
#include <memory>

using namespace std;

namespace {
	string Tons(int tons)
	{
		return to_string(tons) + (tons == 1 ? " ton" : " tons");
	}
}



BodymodderPanel::BodymodderPanel(PlayerInfo &player)
	: ShopPanel(player, true)
{
	for(const pair<const string, Bodymod> &it : GameData::Bodymods())
		catalog[it.second.Category()].insert(it.first);
	
	// Add owned licenses
	const string PREFIX = "license: ";
	for(auto &it : player.Conditions())
		if(it.first.compare(0, PREFIX.length(), PREFIX) == 0 && it.second > 0)
		{
			const string name = it.first.substr(PREFIX.length()) + " License";
			const Bodymod *bodymod = GameData::Bodymods().Get(name);
			if(bodymod)
				catalog[bodymod->Category()].insert(name);
		}
	
	if(player.GetPlanet())
		bodymodder = player.GetPlanet()->Bodymodder();
}


	
void BodymodderPanel::Step()
{
	CheckRefill();
	DoHelp("bodymodder");
	ShopPanel::Step();
}



int BodymodderPanel::TileSize() const
{
	return BODYMOD_SIZE;
}



int BodymodderPanel::DrawPlayerSuitInfo(const Point &point)
{
	suitInfo.Update(*playerSuit, player.FleetDepreciation(), day);
	suitInfo.DrawAttributes(point);
	
	return suitInfo.AttributesHeight();
}



bool BodymodderPanel::HasItem(const string &name) const
{
	const Bodymod *bodymod = GameData::Bodymods().Get(name);
	if((bodymodder.Has(bodymod) || player.Stock(bodymod) > 0) && showForSale)
		return true;
	
	if(player.Cargo().Get(bodymod) && (!playerSuit || showForSale))
		return true;
	
	for(const Suit *suit : playerSuits)
		if(suit->BodymodCount(bodymod))
			return true;
	
	if(showForSale && HasLicense(name))
		return true;
	
	return false;
}



void BodymodderPanel::DrawItem(const string &name, const Point &point, int scrollY)
{
	const Bodymod *bodymod = GameData::Bodymods().Get(name);
	zones.emplace_back(point, Point(BODYMOD_SIZE, BODYMOD_SIZE), bodymod, scrollY);
	if(point.Y() + BODYMOD_SIZE / 2 < Screen::Top() || point.Y() - BODYMOD_SIZE / 2 > Screen::Bottom())
		return;
	
	bool isSelected = (bodymod == selectedBodymod);
	bool isOwned = playerSuit && playerSuit->BodymodCount(bodymod);
	DrawBodymod(*bodymod, point, isSelected, isOwned);
	
	// Check if this bodymod is a "license".
	bool isLicense = IsLicense(name);
	int mapSize = bodymod->Get("map");
	
	const Font &font = FontSet::Get(14);
	const Color &bright = *GameData::Colors().Get("bright");
	if(playerSuit || isLicense || mapSize)
	{
		int minCount = numeric_limits<int>::max();
		int maxCount = 0;
		if(isLicense)
			minCount = maxCount = player.GetCondition(LicenseName(name));
		else if(mapSize)
			minCount = maxCount = HasMapped(mapSize);
		else
		{
			for(const Suit *suit : playerSuits)
			{
				int count = suit->BodymodCount(bodymod);
				minCount = min(minCount, count);
				maxCount = max(maxCount, count);
			}
		}
		
		if(maxCount)
		{
			string label = "installed: " + to_string(minCount);
			if(maxCount > minCount)
				label += " - " + to_string(maxCount);
			
			Point labelPos = point + Point(-BODYMOD_SIZE / 2 + 20, BODYMOD_SIZE / 2 - 38);
			font.Draw(label, labelPos, bright);
		}
	}
	// Don't show the "in stock" amount if the bodymod has an unlimited stock or
	// if it is not something that you can buy.
	int stock = 0;
	if(!bodymodder.Has(bodymod) && bodymod->Get("installable") >= 0.)
		stock = max(0, player.Stock(bodymod));
	int cargo = player.Cargo().Get(bodymod);
	
	string message;
	if(cargo && stock)
		message = "in cargo: " + to_string(cargo) + ", in stock: " + to_string(stock);
	else if(cargo)
		message = "in cargo: " + to_string(cargo);
	else if(stock)
		message = "in stock: " + to_string(stock);
	else if(!bodymodder.Has(bodymod))
		message = "(not sold here)";
	if(!message.empty())
	{
		Point pos = point + Point(
			BODYMOD_SIZE / 2 - 20 - font.Width(message),
			BODYMOD_SIZE / 2 - 24);
		font.Draw(message, pos, bright);
	}
}



int BodymodderPanel::DividerOffset() const
{
	return 80;
}



int BodymodderPanel::DetailWidth() const
{
	return 3 * bodymodInfo.PanelWidth();
}



int BodymodderPanel::DrawDetails(const Point &center)
{
	if(!selectedBodymod)
		return 0;
	
	bodymodInfo.Update(*selectedBodymod, player, CanSell());
	Point offset(bodymodInfo.PanelWidth(), 0.);
	
	bodymodInfo.DrawDescription(center - offset * 1.5 - Point(0., 10.));
	bodymodInfo.DrawRequirements(center - offset * .5 - Point(0., 10.));
	bodymodInfo.DrawAttributes(center + offset * .5 - Point(0., 10.));
	
	return bodymodInfo.MaximumHeight();
}



bool BodymodderPanel::CanBuy() const
{
	if(!planet || !selectedBodymod)
		return false;
	
	bool isInCargo = player.Cargo().Get(selectedBodymod) && playerSuit;
	if(!(bodymodder.Has(selectedBodymod) || player.Stock(selectedBodymod) > 0 || isInCargo))
		return false;
	
	int mapSize = selectedBodymod->Get("map");
	if(mapSize > 0 && HasMapped(mapSize))
		return false;
	
	// Determine what you will have to pay to buy this bodymod.
	int64_t cost = player.StockDepreciation().Value(selectedBodymod, day);
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(selectedBodymod);
	if(licenseCost < 0)
		return false;
	cost += licenseCost;
	// If you have this in your cargo hold, installing it is free.
	if(cost > player.Accounts().Credits() && !isInCargo)
		return false;
	
	if(HasLicense(selectedBodymod->Name()))
		return false;
	
	if(!playerSuit)
	{
		double mass = selectedBodymod->Mass();
		return (!mass || player.Cargo().Free() >= mass);
	}
	
	for(const Suit *suit : playerSuits)
		if(SuitCanBuy(suit, selectedBodymod))
			return true;
	
	return false;
}



void BodymodderPanel::Buy(bool fromCargo)
{
	int64_t licenseCost = LicenseCost(selectedBodymod);
	if(licenseCost)
	{
		player.Accounts().AddCredits(-licenseCost);
		for(const string &licenseName : selectedBodymod->Licenses())
			if(!player.GetCondition("license: " + licenseName))
				player.Conditions()["license: " + licenseName] = true;
	}
	
	int modifier = Modifier();
	for(int i = 0; i < modifier && CanBuy(); ++i)
	{
		// Special case: maps.
		int mapSize = selectedBodymod->Get("map");
		if(mapSize > 0)
		{
			if(!HasMapped(mapSize))
			{
				DistanceMap distance(player.GetSystem(), mapSize);
				for(const System *system : distance.Systems())
					if(!player.HasVisited(system))
						player.Visit(system);
				int64_t price = player.StockDepreciation().Value(selectedBodymod, day);
				player.Accounts().AddCredits(-price);
			}
			return;
		}
		
		// Special case: licenses.
		if(IsLicense(selectedBodymod->Name()))
		{
			auto &entry = player.Conditions()[LicenseName(selectedBodymod->Name())];
			if(entry <= 0)
			{
				entry = true;
				int64_t price = player.StockDepreciation().Value(selectedBodymod, day);
				player.Accounts().AddCredits(-price);
			}
			return;
		}
		
		if(!playerSuit)
		{
			player.Cargo().Add(selectedBodymod);
			int64_t price = player.StockDepreciation().Value(selectedBodymod, day);
			player.Accounts().AddCredits(-price);
			player.AddStock(selectedBodymod, -1);
			continue;
		}
		
		// Find the suits with the fewest number of these bodymods.
		const vector<Suit *> suitsToBodymod = GetSuitsToBodymod(true);
		
		for(Suit *suit : suitsToBodymod)
		{
			if(!CanBuy())
				return;
		
			if(player.Cargo().Get(selectedBodymod))
				player.Cargo().Remove(selectedBodymod);
			else if(fromCargo || !(player.Stock(selectedBodymod) > 0 || bodymodder.Has(selectedBodymod)))
				break;
			else
			{
				int64_t price = player.StockDepreciation().Value(selectedBodymod, day);
				player.Accounts().AddCredits(-price);
				player.AddStock(selectedBodymod, -1);
			}
			suit->AddBodymod(selectedBodymod, 1);
//			int required = selectedBodymod->Get("required crew");
//			if(required && suit->Crew() + required <= static_cast<int>(suit->Attributes().Get("bunks")))
//				suit->AddCrew(required);
			suit->Recharge();
		}
	}
}



void BodymodderPanel::FailBuy() const
{
	if(!selectedBodymod)
		return;
	
	int64_t cost = player.StockDepreciation().Value(selectedBodymod, day);
	int64_t credits = player.Accounts().Credits();
	bool isInCargo = player.Cargo().Get(selectedBodymod);
	if(!isInCargo && cost > credits)
	{
		GetUI()->Push(new Dialog("You cannot buy this bodymod, because it costs "
			+ Format::Credits(cost) + " credits, and you only have "
			+ Format::Credits(credits) + "."));
		return;
	}
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(selectedBodymod);
	if(licenseCost < 0)
	{
		GetUI()->Push(new Dialog(
			"You cannot buy this bodymod, because it requires a license that you don't have."));
		return;
	}
	if(!isInCargo && cost + licenseCost > credits)
	{
		GetUI()->Push(new Dialog(
			"You don't have enough money to buy this bodymod, because it will cost you an extra "
			+ Format::Credits(licenseCost) + " credits to buy the necessary licenses."));
		return;
	}
	
	if(!(bodymodder.Has(selectedBodymod) || player.Stock(selectedBodymod) > 0 || isInCargo))
	{
		GetUI()->Push(new Dialog("You cannot buy this bodymod here. "
			"It is being shown in the list because you have one installed in your suit, "
			"but this " + planet->Noun() + " does not sell them."));
		return;
	}
	
	if(selectedBodymod->Get("map"))
	{
		GetUI()->Push(new Dialog("You have already mapped all the systems shown by this map, "
			"so there is no reason to buy another."));
		return;
	}
	
	if(HasLicense(selectedBodymod->Name()))
	{
		GetUI()->Push(new Dialog("You already have one of these licenses, "
			"so there is no reason to buy another."));
		return;
	}
	
	if(!playerSuit)
		return;
	
	double bodymodNeeded = -selectedBodymod->Get("bodymod space");
	double bodymodSpace = playerSuit->Attributes().Get("bodymod space");
	if(bodymodNeeded > bodymodSpace)
	{
		string need =  to_string(bodymodNeeded) + (bodymodNeeded != 1. ? "tons" : "ton");
		GetUI()->Push(new Dialog("You cannot install this bodymod, because it takes up "
			+ Tons(bodymodNeeded) + " of bodymod space, and this suit has "
			+ Tons(bodymodSpace) + " free."));
		return;
	}
//
//	double weaponNeeded = -selectedBodymod->Get("weapon capacity");
//	double weaponSpace = playerSuit->Attributes().Get("weapon capacity");
//	if(weaponNeeded > weaponSpace)
//	{
//		GetUI()->Push(new Dialog("Only part of your suit's bodymod capacity is usable for weapons. "
//			"You cannot install this bodymod, because it takes up "
//			+ Tons(weaponNeeded) + " of weapon space, and this suit has "
//			+ Tons(weaponSpace) + " free."));
//		return;
//	}
//
//	double engineNeeded = -selectedBodymod->Get("engine capacity");
//	double engineSpace = playerSuit->Attributes().Get("engine capacity");
//	if(engineNeeded > engineSpace)
//	{
//		GetUI()->Push(new Dialog("Only part of your suit's bodymod capacity is usable for engines. "
//			"You cannot install this bodymod, because it takes up "
//			+ Tons(engineNeeded) + " of engine space, and this suit has "
//			+ Tons(engineSpace) + " free."));
//		return;
//	}
//
//	if(selectedBodymod->Category() == "Ammunition")
//	{
//		if(!playerSuit->BodymodCount(selectedBodymod))
//			GetUI()->Push(new Dialog("This bodymod is ammunition for a weapon. "
//				"You cannot install it without first installing the appropriate weapon."));
//		else
//			GetUI()->Push(new Dialog("You already have the maximum amount of ammunition for this weapon. "
//				"If you want to install more ammunition, you must first install another of these weapons."));
//		return;
//	}
//
//	int mountsNeeded = -selectedBodymod->Get("turret mounts");
//	int mountsFree = playerSuit->Attributes().Get("turret mounts");
//	if(mountsNeeded && !mountsFree)
//	{
//		GetUI()->Push(new Dialog("This weapon is designed to be installed on a turret mount, "
//			"but your suit does not have any unused turret mounts available."));
//		return;
//	}
//
//	int gunsNeeded = -selectedBodymod->Get("gun ports");
//	int gunsFree = playerSuit->Attributes().Get("gun ports");
//	if(gunsNeeded && !gunsFree)
//	{
//		GetUI()->Push(new Dialog("This weapon is designed to be installed in a gun port, "
//			"but your suit does not have any unused gun ports available."));
//		return;
//	}
	
	if(selectedBodymod->Get("installable") < 0.)
	{
		GetUI()->Push(new Dialog("This item is not an bodymod that can be installed in a suit."));
		return;
	}
	
	if(!playerSuit->Attributes().CanAdd(*selectedBodymod, 1))
	{
		GetUI()->Push(new Dialog("You cannot install this bodymod in your suit, "
			"because it would reduce one of your suit's attributes to a negative amount. "
			"For example, it may use up more cargo space than you have left."));
		return;
	}
}



bool BodymodderPanel::CanSell(bool toCargo) const
{
	if(!planet || !selectedBodymod)
		return false;
	
	if(!toCargo && player.Cargo().Get(selectedBodymod))
		return true;
	
	for(const Suit *suit : playerSuits)
		if(SuitCanSell(suit, selectedBodymod))
			return true;
	
	return false;
}



void BodymodderPanel::Sell(bool toCargo)
{
	if(!toCargo && player.Cargo().Get(selectedBodymod))
	{
		player.Cargo().Remove(selectedBodymod);
		int64_t price = player.FleetDepreciation().Value(selectedBodymod, day);
		player.Accounts().AddCredits(price);
		player.AddStock(selectedBodymod, 1);
	}
	else
	{
		// Get the suits that have the most of this bodymod installed.
		const vector<Suit *> suitsToBodymod = GetSuitsToBodymod();
		
		for(Suit *suit : suitsToBodymod)
		{
			suit->AddBodymod(selectedBodymod, -1);
//			if(selectedBodymod->Get("required crew"))
//				suit->AddCrew(-selectedBodymod->Get("required crew"));
			suit->Recharge();
			if(toCargo && player.Cargo().Add(selectedBodymod))
			{
				// Transfer to cargo completed.
			}
			else
			{
				int64_t price = player.FleetDepreciation().Value(selectedBodymod, day);
				player.Accounts().AddCredits(price);
				player.AddStock(selectedBodymod, 1);
			}
			
//			const Bodymod *ammo = selectedBodymod->Ammo();
//			if(ammo && ship->BodymodCount(ammo))
//			{
//				// Determine how many of this ammo I must sell to also sell the launcher.
//				int mustSell = 0;
//				for(const pair<const char *, double> &it : ship->Attributes().Attributes())
//					if(it.second < 0.)
//						mustSell = max<int>(mustSell, it.second / ammo->Get(it.first));
//
//				if(mustSell)
//				{
//					ship->AddBodymod(ammo, -mustSell);
//					if(toCargo)
//						mustSell -= player.Cargo().Add(ammo, mustSell);
//					if(mustSell)
//					{
//						int64_t price = player.FleetDepreciation().Value(ammo, day, mustSell);
//						player.Accounts().AddCredits(price);
//						player.AddStock(ammo, mustSell);
//					}
//				}
//			}
		}
	}
}



void BodymodderPanel::FailSell(bool toCargo) const
{
	const string &verb = toCargo ? "uninstall" : "sell";
	if(!planet || !selectedBodymod)
		return;
	else if(selectedBodymod->Get("map"))
		GetUI()->Push(new Dialog("You cannot " + verb + " maps. Once you buy one, it is yours permanently."));
	else if(HasLicense(selectedBodymod->Name()))
		GetUI()->Push(new Dialog("You cannot " + verb + " licenses. Once you obtain one, it is yours permanently."));
	else
	{
		bool hasBodymod = !toCargo && player.Cargo().Get(selectedBodymod);
		for(const Suit *suit : playerSuits)
			if(suit->BodymodCount(selectedBodymod))
			{
				hasBodymod = true;
				break;
			}
		if(!hasBodymod)
			GetUI()->Push(new Dialog("You do not have any of these bodymods to " + verb + "."));
		else
		{
			for(const Suit *suit : playerSuits)
				for(const pair<const char *, double> &it : selectedBodymod->Attributes())
					if(suit->Attributes().Get(it.first) < it.second)
					{
						for(const auto &sit : suit->Bodymods())
							if(sit.first->Get(it.first) < 0.)
							{
								GetUI()->Push(new Dialog("You cannot " + verb + " this bodymod, "
									"because that would cause your suit's \"" + it.first +
									"\" value to be reduced to less than zero. "
									"To " + verb + " this bodymod, you must " + verb + " the " +
									sit.first->Name() + " bodymod first."));
								return;
							}
						GetUI()->Push(new Dialog("You cannot " + verb + " this bodymod, "
							"because that would cause your suit's \"" + it.first +
							"\" value to be reduced to less than zero."));
						return;
					}
			GetUI()->Push(new Dialog("You cannot " + verb + " this bodymod, "
				"because something else in your suit depends on it."));
		}
	}
}



bool BodymodderPanel::ShouldHighlight(const Suit *suit)
{
	if(!selectedBodymod)
		return false;
	
	if(hoverButton == 'b')
		return CanBuy() && SuitCanBuy(suit, selectedBodymod);
	else if(hoverButton == 's')
		return CanSell() && SuitCanSell(suit, selectedBodymod);
	
	return false;
}



void BodymodderPanel::DrawKey()
{
	const Sprite *back = SpriteSet::Get("ui/bodymodder key");
	SpriteShader::Draw(back, Screen::BottomLeft() + .5 * Point(back->Width(), -back->Height()));
	
	Font font = FontSet::Get(14);
	Color color[2] = {*GameData::Colors().Get("medium"), *GameData::Colors().Get("bright")};
	const Sprite *box[2] = {SpriteSet::Get("ui/unchecked"), SpriteSet::Get("ui/checked")};
	
	Point pos = Screen::BottomLeft() + Point(10., -30.);
	Point off = Point(10., -.5 * font.Height());
	SpriteShader::Draw(box[showForSale], pos);
	font.Draw("Show bodymods for sale", pos + off, color[showForSale]);
	AddZone(Rectangle(pos + Point(80., 0.), Point(180., 20.)), [this](){ ToggleForSale(); });
	
	bool showCargo = !playerSuit;
	pos.Y() += 20.;
	SpriteShader::Draw(box[showCargo], pos);
	font.Draw("Show bodymods in cargo", pos + off, color[showCargo]);
	AddZone(Rectangle(pos + Point(80., 0.), Point(180., 20.)), [this](){ ToggleCargo(); });
}



void BodymodderPanel::ToggleForSale()
{
	showForSale = !showForSale;
	
	ShopPanel::ToggleForSale();
}



void BodymodderPanel::ToggleCargo()
{
	if(playerSuit)
	{
		previousSuit = playerSuit;
		playerSuit = nullptr;
		previousSuits = playerSuits;
		playerSuits.clear();
	}
	else if(previousSuit)
	{
		playerSuit = previousSuit;
		playerSuits = previousSuits;
	}
	else
	{
		playerSuit = player.Flagsuit();
		if(playerSuit)
			playerSuits.insert(playerSuit);
	}
	
	ShopPanel::ToggleCargo();
}



bool BodymodderPanel::SuitCanBuy(const Suit *suit, const Bodymod *bodymod)
{
	return (suit->Attributes().CanAdd(*bodymod, 1) > 0);
}



bool BodymodderPanel::SuitCanSell(const Suit *suit, const Bodymod *bodymod)
{
	if(!suit->BodymodCount(bodymod))
		return false;
	
	// If this bodymod requires ammo, check if we could sell it if we sold all
	// the ammo for it first.
//	const Bodymod *ammo = bodymod->Ammo();
//	if(ammo && suit->BodymodCount(ammo))
//	{
//		Bodymod attributes = suit->Attributes();
//		attributes.Add(*ammo, -suit->BodymodCount(ammo));
//		return attributes.CanAdd(*bodymod, -1);
//	}
//
	// Now, check whether this suit can sell this bodymod.
	return suit->Attributes().CanAdd(*bodymod, -1);
}



void BodymodderPanel::DrawBodymod(const Bodymod &bodymod, const Point &center, bool isSelected, bool isOwned)
{
	const Sprite *thumbnail = bodymod.Thumbnail();
	const Sprite *back = SpriteSet::Get(
		isSelected ? "ui/bodymodder selected" : "ui/bodymodder unselected");
	SpriteShader::Draw(back, center);
	SpriteShader::Draw(thumbnail, center);
	
	// Draw the bodymod name.
	const string &name = bodymod.Name();
	const Font &font = FontSet::Get(14);
	Point offset(-.5f * font.Width(name), -.5f * BODYMOD_SIZE + 10.f);
	font.Draw(name, center + offset, Color((isSelected | isOwned) ? .8 : .5, 0.));
}



bool BodymodderPanel::HasMapped(int mapSize) const
{
	DistanceMap distance(player.GetSystem(), mapSize);
	for(const System *system : distance.Systems())
		if(!player.HasVisited(system))
			return false;
	
	return true;
}



bool BodymodderPanel::IsLicense(const string &name) const
{
	static const string &LICENSE = " License";
	if(name.length() < LICENSE.length())
		return false;
	if(name.compare(name.length() - LICENSE.length(), LICENSE.length(), LICENSE))
		return false;
	
	return true;
}



bool BodymodderPanel::HasLicense(const string &name) const
{
	return (IsLicense(name) && player.GetCondition(LicenseName(name)) > 0);
}



string BodymodderPanel::LicenseName(const string &name) const
{
	static const string &LICENSE = " License";
	return "license: " + name.substr(0, name.length() - LICENSE.length());
}



void BodymodderPanel::CheckRefill()
{
	if(checkedRefill)
		return;
	checkedRefill = true;
	
	int count = 0;
	map<const Bodymod *, int> needed;
//	for(const shared_ptr<Suit> &suit : player.Suits())
//	{
//		if(suit->GetSystem() != player.GetSystem() || suit->IsDisabled())
//			continue;
//
//		++count;
//		set<const Bodymod *> toRefill;
//		for(const Hardpoint &it : suit->Weapons())
//			if(it.GetBodymod() && it.GetBodymod()->Ammo())
//				toRefill.insert(it.GetBodymod()->Ammo());
//
//		for(const Bodymod *bodymod : toRefill)
//		{
//			int amount = suit->Attributes().CanAdd(*bodymod, numeric_limits<int>::max());
//			if(amount > 0 && (bodymodder.Has(bodymod) || player.Stock(bodymod) > 0 || player.Cargo().Get(bodymod)))
//				needed[bodymod] += amount;
//		}
//	}
	
	int64_t cost = 0;
	for(auto &it : needed)
	{
		// Don't count cost of anything installed from cargo.
		it.second = max(0, it.second - player.Cargo().Get(it.first));
		if(!bodymodder.Has(it.first))
			it.second = min(it.second, max(0, player.Stock(it.first)));
		cost += player.StockDepreciation().Value(it.first, day, it.second);
	}
//	if(!needed.empty() && cost < player.Accounts().Credits())
//	{
//		string message = "Do you want to reload all the ammunition for your suit";
//		message += (count == 1) ? "?" : "s?";
//		if(cost)
//			message += " It will cost " + Format::Credits(cost) + " credits.";
//		GetUI()->Push(new Dialog(this, &BodymodderPanel::Refill, message));
//	}
}



void BodymodderPanel::Refill()
{
//	for(const shared_ptr<Suit> &suit : player.Suits())
//	{
//		if(suit->GetSystem() != player.GetSystem() || suit->IsDisabled())
//			continue;
//
//		set<const Bodymod *> toRefill;
//		for(const Hardpoint &it : suit->Weapons())
//			if(it.GetBodymod() && it.GetBodymod()->Ammo())
//				toRefill.insert(it.GetBodymod()->Ammo());
//
//		for(const Bodymod *bodymod : toRefill)
//		{
//			int neededAmmo = suit->Attributes().CanAdd(*bodymod, numeric_limits<int>::max());
//			if(neededAmmo > 0)
//			{
//				// Fill first from any stockpiles in cargo.
//				int fromCargo = player.Cargo().Remove(bodymod, neededAmmo);
//				neededAmmo -= fromCargo;
//				// Then, buy at reduced (or full) price.
//				int available = bodymodder.Has(bodymod) ? neededAmmo : min<int>(neededAmmo, max<int>(0, player.Stock(bodymod)));
//				if(neededAmmo && available > 0)
//				{
//					int64_t price = player.StockDepreciation().Value(bodymod, day, available);
//					player.Accounts().AddCredits(-price);
//					player.AddStock(bodymod, -available);
//				}
//				suit->AddBodymod(bodymod, available + fromCargo);
//			}
//		}
//	}
}



// Determine which suits of the selected suits should be referenced in this
// iteration of Buy / Sell.
const vector<Suit *> BodymodderPanel::GetSuitsToBodymod(bool isBuy) const
{
	vector<Suit *> suitsToBodymod;
	int compareValue = isBuy ? numeric_limits<int>::max() : 0;
	int compareMod = 2 * isBuy - 1;
	for(Suit *suit : playerSuits)
	{
		if((isBuy && !SuitCanBuy(suit, selectedBodymod))
				|| (!isBuy && !SuitCanSell(suit, selectedBodymod)))
			continue;
		
		int count = suit->BodymodCount(selectedBodymod);
		if(compareMod * count < compareMod * compareValue)
		{
			suitsToBodymod.clear();
			compareValue = count;
		}
		if(count == compareValue)
			suitsToBodymod.push_back(suit);
	}
	
	return suitsToBodymod;
}
